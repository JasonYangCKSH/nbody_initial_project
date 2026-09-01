#include "bench_runner.h"
#include "simulation.h"
#include "scenario.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

// 固定參數（見檔案開頭需求說明）
constexpr float kDt = 1.0f / 60.0f;
constexpr float kBoxSize = 60.0f;
constexpr int kParticleNum = 2000;
constexpr float kRadius = 1.0f;
constexpr float kSpeed = 1.5f;
constexpr float kAcc = 0.0f;
constexpr int kTotalFrames = 300;
constexpr int kRepeatCount = 5;

const std::vector<int> kLeafCapacities = {2, 4, 8, 16, 32, 64};
const std::vector<int> kMaxDepths = {4, 6, 8, 10, 12};

// ---------------------------------------------------------------------------
// Part B 用的常數：K 附近的局部搜索（hasSkin=true）。
//
// Octree 的結構參數是 (leafCapacity, maxDepth) 兩個維度，Part A 完整格點就已經
// kLeafCapacities.size() x kMaxDepths.size() = 30 組合；如果每個 K 都重掃一次
// 完整格點，11 個 K 要再乘 11 倍，成本太高。改成以 Part A 找到的最佳值為中心，
// 每個維度只看鄰近 kLocalSearchRadius 格（見 neighborsAround()），單一 K 最多
// 只需要 (2*radius+1)^2 組合，用局部搜索取代整組重掃。
//
// kSkinTotalFrames 沿用 phase2_k_sweep.cpp 的 1000，理由同 phase1_structural_grid.cpp。
constexpr int kSkinTotalFrames = 1000;
constexpr int kLocalSearchRadius = 1;
const std::vector<float> kKValues = {0.0f, 1.0f, 2.0f, 5.0f, 10.0f, 20.0f, 50.0f, 100.0f};

// 在 sorted 裡找 center 附近 ±radius 個索引位置對應的值（clamp 在合法範圍內，
// 不會重複）。center 若不在 sorted 裡（理論上不會發生，Part A 的最佳值一定
// 來自 kLeafCapacities/kMaxDepths 本身），退回以索引 0 為中心。
template <typename T>
std::vector<T> neighborsAround(const std::vector<T>& sorted, T center, int radius) {
    auto it = std::find(sorted.begin(), sorted.end(), center);
    const int idx = (it != sorted.end()) ? static_cast<int>(it - sorted.begin()) : 0;

    std::vector<T> result;
    for (int d = -radius; d <= radius; ++d) {
        const int i = idx + d;
        if (i < 0 || i >= static_cast<int>(sorted.size())) continue;
        result.push_back(sorted[i]);
    }
    return result;
}

struct ScenarioSpec {
    std::string name;
    unsigned seed;
};

const std::vector<ScenarioSpec> kScenarios = {
    {"uniform_cloud", 100},
    //{"explosion", 200},
};

std::vector<Particle> buildScenario(const ScenarioSpec& spec) {
    if (spec.name == "uniform_cloud") {
        return scenario::uniformCloud(kParticleNum, kBoxSize, kRadius, kSpeed, kAcc, spec.seed);
    }
    // explosion() 沒有 acc 參數（見 scenario.h），kAcc 固定為 0 對這個 scenario 不影響。
    return scenario::explosion(kParticleNum, kBoxSize, kRadius, kSpeed, spec.seed);
}

std::string formatFixed(double value, int precision) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << value;
    return oss.str();
}

struct BestCombo {
    bool found = false;
    int leafCapacity = 0;
    int maxDepth = 0;
    double avgTotalTimeS = 0.0;
};

}  // namespace

int main() {
    using namespace bench_runner;

    const std::vector<CsvColumn> columns = {
        {"scenario", 14, true},
        {"leaf_capacity", 13, false},
        {"max_depth", 9, false},
        {"avg_total_time_s", 16, false},
        {"total_time_std_s", 16, false},
        {"avg_broad_phase_time_s", 22, false},
        {"avg_narrow_phase_time_s", 23, false},
        {"avg_candidates_per_frame", 24, false},
        {"correctness_ok", 14, false},
        {"repeat_count", 12, false},
    };

    std::ofstream csv("phase1_octree_summary.csv");
    writeCsvHeader(csv, columns);

    BruteForceCache bfCache;

    // ---------------------------------------------------------------------------
    // Part A：hasSkin=false 的結構參數 baseline 掃描（原本就有的邏輯，不動）。
    // ---------------------------------------------------------------------------

    const size_t totalCombos = kScenarios.size() * kLeafCapacities.size() * kMaxDepths.size();
    size_t comboIndex = 0;

    std::unordered_map<std::string, BestCombo> bestPerScenario;
    for (const auto& s : kScenarios) bestPerScenario[s.name] = BestCombo();

    for (const auto& spec : kScenarios) {
        std::vector<Particle> particles = buildScenario(spec);

        for (int leafCapacity : kLeafCapacities) {
            for (int maxDepth : kMaxDepths) {
                ++comboIndex;

                auto t0 = std::chrono::steady_clock::now();

                SimulationConfig cfg(
                    kDt, /*K=*/0.0f, /*hasSkin=*/false, Method::Octree,
                    /*cellSize=*/1.0f, maxDepth, leafCapacity, kBoxSize
                );

                RunResult perf = runAndAverage(particles, cfg, kTotalFrames, kRepeatCount);
                // 計時區塊到這裡結束，正確性驗證獨立於計時之外進行（見 bench_runner.h）。
                CorrectnessCheck check =
                    verifyAgainstBruteForce(particles, cfg, kTotalFrames, bfCache, spec.name, spec.seed);

                auto t1 = std::chrono::steady_clock::now();
                double comboElapsedS = std::chrono::duration<double>(t1 - t0).count();

                if (!check.allMatch) {
                    std::cerr << "[WARN] correctness mismatch: scenario=" << spec.name
                              << " leafCapacity=" << leafCapacity << " maxDepth=" << maxDepth
                              << " firstMismatchFrame=" << check.firstMismatchFrame << "\n";
                }

                std::vector<std::string> row = {
                    spec.name,
                    std::to_string(leafCapacity),
                    std::to_string(maxDepth),
                    formatFixed(perf.totalTimeS, 6),
                    formatFixed(perf.totalTimeStdS, 6),
                    formatFixed(perf.broadPhaseTimeS, 6),
                    formatFixed(perf.narrowPhaseTimeS, 6),
                    formatFixed(perf.avgCandidatesPerFrame, 2),
                    check.allMatch ? "1" : "0",
                    std::to_string(kRepeatCount),
                };
                writeCsvRow(csv, columns, row);

                BestCombo& best = bestPerScenario[spec.name];
                if (!best.found || perf.totalTimeS < best.avgTotalTimeS) {
                    best.found = true;
                    best.leafCapacity = leafCapacity;
                    best.maxDepth = maxDepth;
                    best.avgTotalTimeS = perf.totalTimeS;
                }

                std::cerr << "[" << comboIndex << "/" << totalCombos << "] "
                          << "scenario=" << spec.name << " leafCapacity=" << leafCapacity
                          << " maxDepth=" << maxDepth
                          << " avg_total_time_s=" << formatFixed(perf.totalTimeS, 6)
                          << " correctness_ok=" << (check.allMatch ? 1 : 0)
                          << " elapsed=" << formatFixed(comboElapsedS, 2) << "s\n";
            }
        }
    }

    csv.close();

    std::cerr << "\n=== phase1_structural_octree: best (leafCapacity, maxDepth) by avg_total_time_s"
                 " (hasSkin=false baseline) ===\n";
    for (const auto& spec : kScenarios) {
        const BestCombo& best = bestPerScenario[spec.name];
        std::cerr << spec.name << ": leafCapacity=" << best.leafCapacity
                   << " maxDepth=" << best.maxDepth
                   << " avg_total_time_s=" << formatFixed(best.avgTotalTimeS, 6) << "\n";
    }

    // ---------------------------------------------------------------------------
    // Part B：K 附近的局部搜索（hasSkin=true）。
    // 以 Part A 找到的 baseline 為中心，取代整組重掃，得到「K -> 最佳
    // (leafCapacity, maxDepth)」這條關係，才是該套進 phase2 的校準值。
    // ---------------------------------------------------------------------------

    std::cerr << "\n=== Part B: octree + skin，K 附近的局部搜索 ===\n";

    const std::vector<CsvColumn> skinColumns = {
        {"scenario", 14, true},
        {"K", 8, false},
        {"leaf_capacity", 13, false},
        {"max_depth", 9, false},
        {"avg_total_time_s", 16, false},
        {"total_time_std_s", 16, false},
        {"avg_broad_phase_time_s", 22, false},
        {"avg_narrow_phase_time_s", 23, false},
        {"avg_candidates_per_frame", 24, false},
        {"rebuild_count", 14, false},
        {"correctness_ok", 14, false},
        {"repeat_count", 12, false},
    };

    std::ofstream skinCsv("phase1_octree_skin_sweep.csv");
    writeCsvHeader(skinCsv, skinColumns);

    struct BestComboOctree {
        bool found = false;
        int leafCapacity = 0;
        int maxDepth = 0;
        double avgTotalTimeS = 0.0;
    };

    // 每個 scenario、每個 K 底下的最佳 (leafCapacity, maxDepth) —— 這才是該套進
    // phase2 的校準值（K 依賴），不是 Part A 那個跟 K 無關的單一組合。
    std::unordered_map<std::string, std::unordered_map<float, BestComboOctree>> bestByScenarioAndK;

    // 每個 K 的組合數取決於局部搜索鄰域大小（邊界值附近鄰域會比較小），先算一次
    // 總數方便印進度。
    size_t totalSkinCombos = 0;
    for (const auto& spec : kScenarios) {
        const BestCombo& base = bestPerScenario[spec.name];
        const auto leafNeighbors = neighborsAround(kLeafCapacities, base.leafCapacity, kLocalSearchRadius);
        const auto depthNeighbors = neighborsAround(kMaxDepths, base.maxDepth, kLocalSearchRadius);
        totalSkinCombos += kKValues.size() * leafNeighbors.size() * depthNeighbors.size();
    }
    size_t skinComboIndex = 0;

    for (const auto& spec : kScenarios) {
        std::vector<Particle> particles = buildScenario(spec);

        const BestCombo& base = bestPerScenario[spec.name];
        const auto leafNeighbors = neighborsAround(kLeafCapacities, base.leafCapacity, kLocalSearchRadius);
        const auto depthNeighbors = neighborsAround(kMaxDepths, base.maxDepth, kLocalSearchRadius);
        std::cerr << "[Part B] scenario=" << spec.name << " local search 中心=(leafCapacity=" << base.leafCapacity
                  << ", maxDepth=" << base.maxDepth << "), leafCapacity 鄰域={";
        for (int v : leafNeighbors) std::cerr << v << " ";
        std::cerr << "}, maxDepth 鄰域={";
        for (int v : depthNeighbors) std::cerr << v << " ";
        std::cerr << "}\n";

        auto& bestByK = bestByScenarioAndK[spec.name];

        for (float K : kKValues) {
            BestComboOctree bestForK;

            for (int leafCapacity : leafNeighbors) {
                for (int maxDepth : depthNeighbors) {
                    ++skinComboIndex;

                    auto t0 = std::chrono::steady_clock::now();

                    SimulationConfig cfg(
                        kDt, K, /*hasSkin=*/true, Method::Octree,
                        /*cellSize=*/1.0f, maxDepth, leafCapacity, kBoxSize
                    );

                    RunResult perf = runAndAverage(particles, cfg, kSkinTotalFrames, kRepeatCount);
                    // 計時區塊到這裡結束，正確性驗證獨立於計時之外進行（見 bench_runner.h）。
                    CorrectnessCheck check =
                        verifyAgainstBruteForce(particles, cfg, kSkinTotalFrames, bfCache, spec.name, spec.seed);

                    auto t1 = std::chrono::steady_clock::now();
                    double comboElapsedS = std::chrono::duration<double>(t1 - t0).count();

                    if (!check.allMatch) {
                        std::cerr << "[WARN] correctness mismatch: scenario=" << spec.name << " K=" << K
                                  << " leafCapacity=" << leafCapacity << " maxDepth=" << maxDepth
                                  << " firstMismatchFrame=" << check.firstMismatchFrame << "\n";
                    }

                    std::vector<std::string> row = {
                        spec.name,
                        formatFixed(K, 0),
                        std::to_string(leafCapacity),
                        std::to_string(maxDepth),
                        formatFixed(perf.totalTimeS, 6),
                        formatFixed(perf.totalTimeStdS, 6),
                        formatFixed(perf.broadPhaseTimeS, 6),
                        formatFixed(perf.narrowPhaseTimeS, 6),
                        formatFixed(perf.avgCandidatesPerFrame, 2),
                        std::to_string(perf.rebuildCount),
                        check.allMatch ? "1" : "0",
                        std::to_string(kRepeatCount),
                    };
                    writeCsvRow(skinCsv, skinColumns, row);

                    if (!bestForK.found || perf.totalTimeS < bestForK.avgTotalTimeS) {
                        bestForK.found = true;
                        bestForK.leafCapacity = leafCapacity;
                        bestForK.maxDepth = maxDepth;
                        bestForK.avgTotalTimeS = perf.totalTimeS;
                    }

                    std::cerr << "[" << skinComboIndex << "/" << totalSkinCombos << "] "
                              << "scenario=" << spec.name << " K=" << K << " leafCapacity=" << leafCapacity
                              << " maxDepth=" << maxDepth << " avg_total_time_s=" << formatFixed(perf.totalTimeS, 6)
                              << " rebuilds=" << perf.rebuildCount << " correctness_ok=" << (check.allMatch ? 1 : 0)
                              << " elapsed=" << formatFixed(comboElapsedS, 2) << "s\n";
                }
            }

            bestByK[K] = bestForK;
        }
    }
    skinCsv.close();

    const std::vector<CsvColumn> bestByKColumns = {
        {"scenario", 14, true},
        {"K", 8, false},
        {"best_leaf_capacity", 19, false},
        {"best_max_depth", 15, false},
        {"avg_total_time_s", 16, false},
    };
    std::ofstream bestByKCsv("phase1_octree_best_by_k.csv");
    writeCsvHeader(bestByKCsv, bestByKColumns);

    std::cerr << "\n=== phase1_structural_octree: K -> 最佳 (leafCapacity, maxDepth)"
                 "（這條關係才是該套進 phase2 的校準值，不是單一固定組合）===\n";
    for (const auto& spec : kScenarios) {
        const auto& bestByK = bestByScenarioAndK[spec.name];
        for (float K : kKValues) {
            const BestComboOctree& best = bestByK.at(K);

            writeCsvRow(bestByKCsv, bestByKColumns, {
                spec.name,
                formatFixed(K, 0),
                std::to_string(best.leafCapacity),
                std::to_string(best.maxDepth),
                formatFixed(best.avgTotalTimeS, 6),
            });

            std::cerr << spec.name << ": K=" << formatFixed(K, 0) << " best_leafCapacity=" << best.leafCapacity
                       << " best_maxDepth=" << best.maxDepth
                       << " avg_total_time_s=" << formatFixed(best.avgTotalTimeS, 6) << "\n";
        }
    }
    bestByKCsv.close();

    return 0;
}
