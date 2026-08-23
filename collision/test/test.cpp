// Correctness verification for broad::Octree against broad::bruteForce.
// Two phases:
//   1. Tiny scene (few particles), print both pair lists side by side for
//      manual inspection.
//   2. Larger scenes, statistical cross-check: every pair found by one
//      method must also appear in the other (set equality).
//
// Run this before trusting any Octree performance numbers.
 
#include "../include/particle.h"
#include "../include/broad_phase.h"
 
#include <iostream>
#include <random>
#include <set>
#include <algorithm>
 
using PairSet = std::set<std::pair<int, int>>;
 
static PairSet toSet(const PairList& pairs) {
    PairSet s;
    for (auto& p : pairs) {
        int a = p.first, b = p.second;
        if (a > b) std::swap(a, b);
        s.insert({a, b});
    }
    return s;
}
 
static void printPairList(const std::string& label, const PairList& pairs) {
    std::cout << label << " (" << pairs.size() << " pairs): ";
    for (auto& p : pairs) std::cout << "(" << p.first << "," << p.second << ") ";
    std::cout << "\n";
}
 
// ---------- Phase 1: tiny hand-checkable scene ----------
static void phase1_manualCheck() {
    std::cout << "=== Phase 1: manual check (small scene) ===\n";
 
    // Place particles by hand so you can compute expected pairs on paper.
    // Box centered at origin, halfExtent = 10.
    std::vector<Particle> particles;
    auto addParticle = [&](float x, float y, float z, float r) {
        Particle p{};
        p.pos = {x, y, z};
        p.radius = r;
        p.skin = 0.0f;
        particles.push_back(p);
    };
 
    // Two particles close enough to collide (distance 0.15, radii sum 0.2).
    addParticle(0.0f, 0.0f, 0.0f, 0.1f);      // idx 0
    addParticle(0.15f, 0.0f, 0.0f, 0.1f);     // idx 1
    // One far away particle (no collision with anything).
    addParticle(8.0f, 8.0f, 8.0f, 0.1f);      // idx 2
    // Two particles in a different region, also colliding.
    addParticle(-5.0f, -5.0f, -5.0f, 0.1f);   // idx 3
    addParticle(-5.05f, -5.0f, -5.0f, 0.1f);  // idx 4
 
    PairList bfPairs = broad::bruteForce(particles, /*withSkin=*/false);
 
    broad::Octree tree(glm::vec3(0.0f), 10.0f, /*leafCapacity=*/2, /*maxDepth=*/8);
    PairList octPairs = tree.build(particles, /*withSkin=*/false);
 
    printPairList("brute-force", bfPairs);
    printPairList("octree     ", octPairs);
 
    if (toSet(bfPairs) == toSet(octPairs)) {
        std::cout << "Phase 1 PASS: pair sets match.\n\n";
    } else {
        std::cout << "Phase 1 FAIL: pair sets differ!\n\n";
    }
}
 
// ---------- Phase 2: larger random scenes, set-equality check ----------
static std::vector<Particle> randomCloud(int n, float boxHalfExtent, float radius,
                                          float skin, unsigned seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist(-boxHalfExtent, boxHalfExtent);
    std::vector<Particle> particles(n);
    for (auto& p : particles) {
        p.pos = {dist(rng), dist(rng), dist(rng)};
        p.radius = radius;
        p.skin = skin;
    }
    return particles;
}
 
static bool phase2_crossCheck(int n, float boxHalfExtent, float radius, float skin,
                               bool withSkin, unsigned seed) {
    auto particles = randomCloud(n, boxHalfExtent, radius, skin, seed);
 
    PairList bfPairs = broad::bruteForce(particles, withSkin);
 
    broad::Octree tree(glm::vec3(0.0f), boxHalfExtent, /*leafCapacity=*/8, /*maxDepth=*/12);
    PairList octPairs = tree.build(particles, withSkin);
 
    PairSet bfSet = toSet(bfPairs);
    PairSet octSet = toSet(octPairs);
 
    bool match = (bfSet == octSet);
 
    std::cout << "n=" << n << " withSkin=" << withSkin << " seed=" << seed
              << " | brute-force=" << bfSet.size() << " octree=" << octSet.size()
              << " | " << (match ? "PASS" : "FAIL") << "\n";
 
    if (!match) {
        // Report the discrepancy so you know which direction the bug is in.
        PairSet missingFromOctree, extraInOctree;
        for (auto& p : bfSet) if (!octSet.count(p)) missingFromOctree.insert(p);
        for (auto& p : octSet) if (!bfSet.count(p)) extraInOctree.insert(p);
 
        std::cout << "  missing from octree (" << missingFromOctree.size() << "): ";
        for (auto& p : missingFromOctree) std::cout << "(" << p.first << "," << p.second << ") ";
        std::cout << "\n  extra in octree (" << extraInOctree.size() << "): ";
        for (auto& p : extraInOctree) std::cout << "(" << p.first << "," << p.second << ") ";
        std::cout << "\n";
    }
 
    return match;
}
 
int main() {
    phase1_manualCheck();
 
    std::cout << "=== Phase 2: statistical cross-check ===\n";
    bool allPass = true;
 
    // Sweep particle counts, both with and without skin, multiple seeds.
    std::vector<int> counts = {10, 50, 100, 500, 1000, 2000, 10000};
    std::vector<bool> skinModes = {false, true};
 
    for (bool withSkin : skinModes) {
        for (int n : counts) {
            for (unsigned seed = 1; seed <= 3; ++seed) {
                float radius = 0.1f;
                float skin = withSkin ? 0.3f : 0.0f;
                bool ok = phase2_crossCheck(n, 10.0f, radius, skin, withSkin, seed);
                allPass = allPass && ok;
            }
        }
    }
 
    std::cout << "\n" << (allPass ? "ALL PHASE 2 CHECKS PASSED" : "SOME CHECKS FAILED — see above") << "\n";
    return allPass ? 0 : 1;
}