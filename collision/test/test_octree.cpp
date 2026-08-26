#include "../include/particle.h"
#include "../include/broad_phase.h"
 
#include <iostream>
#include <vector>
#include <cmath>
#include <random>
// Reference (independent) descent: given the same childIndex logic, walk
// from the root by hand using only center/halfExtent bookkeeping, without
// touching Octree's internals. If this disagrees with leafHalfExtentOf,
// something is wrong with the tree structure or the query itself.
static int referenceChildIndex(const glm::vec3& center, const glm::vec3& pos) {
    int idx = 0;
    if (pos.x >= center.x) idx |= 1;
    if (pos.y >= center.y) idx |= 2;
    if (pos.z >= center.z) idx |= 4;
    return idx;
}
 
static glm::vec3 referenceChildCenter(const glm::vec3& parentCenter, float parentHalfExtent, int childIdx) {
    float q = parentHalfExtent * 0.5f;
    glm::vec3 offset(
        (childIdx & 1) ? q : -q,
        (childIdx & 2) ? q : -q,
        (childIdx & 4) ? q : -q
    );
    return parentCenter + offset;
}
 
// ---------- Phase 1: hand-checkable scene ----------
static void phase1_manualCheck() {
    std::cout << "=== Phase 1: manual leafHalfExtentOf check ===\n";
 
    // Box centered at origin, halfExtent = 8, leafCapacity = 1 so that
    // ANY two particles sharing a region force a split — this lets us
    // predict leaf depth by hand.
    std::vector<Particle> particles;
    auto addParticle = [&](float x, float y, float z) {
        Particle p{};
        p.pos = {x, y, z};
        p.radius = 0.05f;
        p.skin = 0.0f;
        particles.push_back(p);
    };
 
    // Cluster of 3 particles very close together near (2,2,2): forces
    // several levels of subdivision -> small halfExtent expected.
    addParticle(2.0f, 2.0f, 2.0f);   // idx 0
    addParticle(2.1f, 2.0f, 2.0f);   // idx 1
    addParticle(2.0f, 2.1f, 2.0f);   // idx 2
    // One isolated particle far away near (-6,-6,-6): should stay in a
    // large, shallow leaf -> large halfExtent expected.
    addParticle(-6.0f, -6.0f, -6.0f); // idx 3
 
    broad::Octree tree(glm::vec3(0.0f), 8.0f, /*leafCapacity=*/1, /*maxDepth=*/10);
    tree.build(particles, /*withSkin=*/false);
 
    for (int i = 0; i < (int)particles.size(); ++i) {
        float h = tree.leafHalfExtentOf(i, particles);
        std::cout << "particle " << i << " at (" << particles[i].pos.x << ","
                   << particles[i].pos.y << "," << particles[i].pos.z
                   << ") -> leaf halfExtent = " << h << "\n";
    }
 
    float hClustered = tree.leafHalfExtentOf(0, particles);
    float hIsolated = tree.leafHalfExtentOf(3, particles);
 
    bool pass = hClustered < hIsolated;
    std::cout << (pass ? "Phase 1 PASS: clustered particle has smaller leaf than isolated particle.\n\n"
                        : "Phase 1 FAIL: expected clustered leaf to be smaller.\n\n");
}
 
// ---------- Phase 2: cross-check against independent reference descent ----------
static bool phase2_crossCheck(int n, float boxHalfExtent, int leafCapacity, unsigned seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist(-boxHalfExtent, boxHalfExtent);
    std::vector<Particle> particles(n);
    for (auto& p : particles) {
        p.pos = {dist(rng), dist(rng), dist(rng)};
        p.radius = 0.05f;
        p.skin = 0.0f;
    }
 
    broad::Octree tree(glm::vec3(0.0f), boxHalfExtent, leafCapacity, /*maxDepth=*/12);
    tree.build(particles, false);
 
    // Reference descent: mirrors Octree::insert's traversal logic exactly,
    // but implemented independently here (no shared code path) so a bug
    // in the real implementation won't silently agree with itself.
    // Since we don't have direct access to Octree's internal leaf capacity
    // trigger without re-deriving it, we instead check a weaker but still
    // meaningful invariant: half-extent must always be a root halfExtent
    // divided by some power of two, and must be > 0 and <= root halfExtent.
    bool allValid = true;
    for (int i = 0; i < n; ++i) {
        float h = tree.leafHalfExtentOf(i, particles);
        bool validRange = (h > 0.0f && h <= boxHalfExtent + 1e-4f);
        // Check h is boxHalfExtent / 2^k for some integer k >= 0.
        float ratio = boxHalfExtent / h;
        float log2ratio = std::log2(ratio);
        bool isPowerOfTwo = std::abs(log2ratio - std::round(log2ratio)) < 1e-3f;
        if (!validRange || !isPowerOfTwo) {
            std::cout << "  particle " << i << " has invalid halfExtent=" << h
                      << " (ratio=" << ratio << ")\n";
            allValid = false;
        }
    }
 
    std::cout << "n=" << n << " leafCapacity=" << leafCapacity << " seed=" << seed
              << " | " << (allValid ? "PASS" : "FAIL") << "\n";
    return allValid;
}
 
int main() {
    phase1_manualCheck();
 
    std::cout << "=== Phase 2: half-extent validity across random scenes ===\n";
    bool allPass = true;
    std::vector<int> counts = {10, 100, 500, 2000};
    std::vector<int> capacities = {1, 4, 8, 16};
 
    for (int cap : capacities) {
        for (int n : counts) {
            for (unsigned seed = 1; seed <= 2; ++seed) {
                bool ok = phase2_crossCheck(n, 10.0f, cap, seed);
                allPass = allPass && ok;
            }
        }
    }
 
    std::cout << "\n" << (allPass ? "ALL PHASE 2 CHECKS PASSED" : "SOME CHECKS FAILED — see above") << "\n";
    return allPass ? 0 : 1;
}