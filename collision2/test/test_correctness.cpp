
#include "simulation.h"
#include "scenario.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr float kBoxSize = 40.0f;
constexpr float kDt = 1.0f / 60.0f;
constexpr int kFrames = 100;
constexpr float kCellSize = 2.0f;
constexpr float maxDepth = 10;
constexpr float leafCapacity = 16;
constexpr float kK = 2.0f;

std::vector<FrameStats> runSim(std::vector<Particle> particles, const SimulationConfig& cfg) {
    Simulation sim(std::move(particles), cfg, kFrames);
    return sim.run();
}

bool matchesTruth(const std::string& label, const std::vector<FrameStats>& truth,
                   const std::vector<FrameStats>& result) {
    for (size_t f = 0; f < truth.size(); ++f) {
        //std::cout << f << ":[" << result[f].candidatePairs.size() << "] ";
        //if ((int)(f + 1) % 10 == 0) std::cout << "\n";
        if (result[f].collisionPairs != truth[f].collisionPairs) {
            std::cout << "[FAIL] " << label << " diverges at frame " << f << "\n";
            return false;
        }
    }
    std::cout << "[PASS] " << label << " matches BruteForce for " << truth.size() << " frames\n";
    return true;
}

}  // namespace

int main() {
    std::vector<Particle> base = scenario::spatialCluster(
        8000, kBoxSize, 1.0f, 1.5f, 5.0f, /*clusterFactor=*/0.0f, /*hotspotSpread=*/kBoxSize * 0.03f,
        /*hotspotCount=*/1, /*seed=*/7
    );

    SimulationConfig bruteCfg(kDt, 0.0f, false, Method::BruteForce, 1.0f, 8, 8, kBoxSize);
    std::vector<FrameStats> truth = runSim(base, bruteCfg);

    SimulationConfig gridCfg(kDt, 0.0f, false, Method::UniformGrid, kCellSize, maxDepth, leafCapacity, kBoxSize);
    SimulationConfig gridSkinCfg(kDt, kK, true, Method::UniformGrid, kCellSize, maxDepth, leafCapacity, kBoxSize);
    SimulationConfig octreeCfg(kDt, 0.0f, false, Method::Octree, 1.0f, maxDepth, leafCapacity, kBoxSize);
    SimulationConfig octreeSkinCfg(kDt, kK, true, Method::Octree, 1.0f, maxDepth, leafCapacity, kBoxSize);

    int passed = 0;
    passed += matchesTruth("UniformGrid", truth, runSim(base, gridCfg));
    passed += matchesTruth("UniformGrid+Skin", truth, runSim(base, gridSkinCfg));
    passed += matchesTruth("Octree", truth, runSim(base, octreeCfg));
    passed += matchesTruth("Octree+Skin", truth, runSim(base, octreeSkinCfg));

    std::cout << "\n" << (passed == 4 ? "[ALL PASS] " : "[SOME FAILED] ") << passed << "/4 methods matched BruteForce\n";
    return passed == 4 ? 0 : 1;
}
