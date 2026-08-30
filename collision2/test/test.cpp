#include <algorithm>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "simulation.h"

using PairList = std::vector<std::pair<int, int>>;

PairList canonicalize(const PairList& pairs) {
    PairList result = pairs;
    for (auto& [a, b] : result) {
        if (a > b) std::swap(a, b);
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

std::string formatPairs(const PairList& pairs) {
    std::ostringstream oss;
    for (size_t i = 0; i < pairs.size(); ++i) {
        if (i != 0) oss << ", ";
        oss << "(" << pairs[i].first << "," << pairs[i].second << ")";
    }
    return oss.str();
}

std::vector<Particle> makeTestParticles() {
    //auto particles = scenario::two_particle_bounce_scenario();
    auto particles = scenario::explosion(100, 100.0f, 1.0f, 2.0f, 1234);
    //particles.resize(3);

    //particles[2].pos = glm::vec3(2.1f, 0.0f, 0.0f);
    //particles[2].vel = glm::vec3(-0.8f, 0.0f, 0.0f);
    //particles[2].acc = glm::vec3(0.0f, 0.0f, 0.0f);
    //particles[2].radius = 0.5f;
    //particles[2].skin = 0.05f;

    return particles;
}

std::vector<StepStats> runSimulation(StructureMode mode,
                                    bool skinEnabled,
                                    const std::vector<Particle>& initial,
                                    int frames) {
    SimConfig cfg;
    cfg.dt = 1.0f / 60.0f;
    cfg.K = 200.0f;
    cfg.cellSize = 1.0f;

    std::unique_ptr<std::variant<broad::UniformGrid, broad::Octree>> structure;
    if (mode == StructureMode::UniformGrid) {
        structure = std::make_unique<std::variant<broad::UniformGrid, broad::Octree>>(
            broad::UniformGrid(1.0f));
    } else if (mode == StructureMode::Octree) {
        structure = std::make_unique<std::variant<broad::UniformGrid, broad::Octree>>(
            broad::Octree(3, 2, 8.0f));
    }

    Simulation sim(frames, mode, std::move(structure), skinEnabled, cfg);
    sim.InitializeParticles(initial);
    return sim.runForFrames(frames);
}

bool compareHistory(const std::string& name,
                   const std::vector<StepStats>& expected,
                   const std::vector<StepStats>& actual) {
    if (expected.size() != actual.size()) {
        std::cout << "FAIL: " << name << "\n";
        std::cout << "  expected frames: " << expected.size() << "\n";
        std::cout << "  actual frames:   " << actual.size() << "\n";
        return false;
    }

    for (size_t frame = 0; frame < expected.size(); ++frame) {
        const auto& e = expected[frame];
        const auto& a = actual[frame];
        PairList expectedPairs = canonicalize(e.collisions);
        PairList actualPairs = canonicalize(a.collisions);

        if (e.collisionCount != a.collisionCount || expectedPairs != actualPairs) {
            std::cout << "FAIL: " << name << " at frame " << frame << "\n";
            std::cout << "  expected collisionCount: " << e.collisionCount << "\n";
            std::cout << "  actual collisionCount:   " << a.collisionCount << "\n";
            std::cout << "  expected pairs: " << formatPairs(expectedPairs) << "\n";
            std::cout << "  actual pairs:   " << formatPairs(actualPairs) << "\n";
            return false;
        }
    }

    std::cout << "PASS: " << name << "\n";
    return true;
}

int main() {
    const int frames = 2000;
    auto initialParticles = makeTestParticles();

    auto expected = runSimulation(StructureMode::BruteForce, false, initialParticles, frames);

    bool ok = true;
    ok &= compareHistory("brute force", expected, expected);

    ok &= compareHistory("uniform grid",
                         expected,
                         runSimulation(StructureMode::UniformGrid, false, initialParticles, frames));

    ok &= compareHistory("uniform grid + skin",
                         expected,
                         runSimulation(StructureMode::UniformGrid, true, initialParticles, frames));

    ok &= compareHistory("octree",
                         expected,
                         runSimulation(StructureMode::Octree, false, initialParticles, frames));

    ok &= compareHistory("octree + skin",
                         expected,
                         runSimulation(StructureMode::Octree, true, initialParticles, frames));

    if (!ok) {
        std::cout << "\nSimulation implementations do not match brute force across multiple frames.\n";
        return 1;
    }

    std::cout << "\nAll simulation modes match brute force across " << frames << " frames.\n";
    return 0;
}
