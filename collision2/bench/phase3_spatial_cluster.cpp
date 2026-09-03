// phase3_spatial_cluster.cpp — Sweep A only：固定 K，掃 clusterFactor，比較
// uniform_grid_skin 與 octree_skin 的 candidate 數量與時間反應。

#include "bench_runner.h"
#include "simulation.h"
#include "scenario.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace bench_runner;

namespace {

constexpr int kParticleNum = 2000;
constexpr float kBoxSize = 60.0f;
constexpr float kRadius = 1.0f;
constexpr float kSpeed = 1.5f;
constexpr float kAcc = 0.5f;
constexpr float kDt = 1.0f / 60.0f;
constexpr int kTotalFrames = 1000;
constexpr int kRepeatCount = 5;
constexpr float kK = 20.0f;
constexpr int kOctreeLeafCapacity = 16;
constexpr int kOctreeMaxDepth = 8;
constexpr float kGridCellSizeRatio = 1.5f; 
constexpr float kGridCellSize = kGridCellSizeRatio * 2.0f * kRadius;
constexpr float kHotspotSpread = kBoxSize * 0.03f;
constexpr int kHotspotCount = 1;

const std::vector<float> kClusterFactors = {0.0f, 0.3f, 0.6f, 0.9f};

std::string fmt(double value, int precision) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << value;
    return oss.str();
}

}  // namespace

int main() {
    const std::vector<CsvColumn> cols = {
        {"cluster_factor", 14, false},
        {"structure_mode", 20, true},
        {"total_time_s", 14, false},
        {"broad_phase_time_s", 18, false},
        {"narrow_phase_time_s", 19, false},
        {"rebuild_count", 14, false},
        {"avg_candidates_per_frame", 24, false},
        {"correctness_ok", 14, false},
    };

    std::ofstream csv("phase3_spatial_cluster_summary.csv");
    writeCsvHeader(csv, cols);

    BruteForceCache bfCache;
    const std::vector<std::string> modes = {"uniform_grid_skin", "octree_skin"};
    const size_t totalCombos = kClusterFactors.size() * modes.size();
    size_t comboIndex = 0;

    for (size_t i = 0; i < kClusterFactors.size(); ++i) {
        const float cf = kClusterFactors[i];
        const unsigned seed = 100u ;//+ static_cast<unsigned>(i);
        const std::vector<Particle> particles = scenario::spatialCluster(
            kParticleNum, kBoxSize, kRadius, kSpeed, kAcc, cf, kHotspotSpread, kHotspotCount, seed
        );
        const std::string cacheKey = "sweepA_cf" + fmt(cf, 2);

        for (const auto& mode : modes) {
            ++comboIndex;
            SimulationConfig cfg(
                kDt, kK, true, mode == "uniform_grid_skin" ? Method::UniformGrid : Method::Octree,
                kGridCellSize, kOctreeMaxDepth, kOctreeLeafCapacity, kBoxSize
            );

            RunResult perf = runAndAverage(particles, cfg, kTotalFrames, kRepeatCount);
            // 計時區塊到這裡結束，正確性驗證獨立於計時之外進行。
            CorrectnessCheck check = verifyAgainstBruteForce(particles, cfg, kTotalFrames, bfCache, cacheKey, seed);

            writeCsvRow(
                csv, cols,
                {fmt(cf, 2), mode, fmt(perf.totalTimeS, 6), fmt(perf.broadPhaseTimeS, 6),
                 fmt(perf.narrowPhaseTimeS, 6), std::to_string(perf.rebuildCount),
                 fmt(perf.avgCandidatesPerFrame, 2), check.allMatch ? "1" : "0"}
            );

            std::cerr << "[" << comboIndex << "/" << totalCombos << "] clusterFactor=" << fmt(cf, 2)
                      << " structure_mode=" << mode << " total_time_s=" << fmt(perf.totalTimeS, 6)
                      << " correctness_ok=" << (check.allMatch ? 1 : 0) << "\n";
        }
    }


    return 0;
}
