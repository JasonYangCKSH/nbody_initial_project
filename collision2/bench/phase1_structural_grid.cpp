#include "bench_runner.h"
#include "simulation.h"
#include "scenario.h"

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

// cellSizeRatio 是相對於粒子直徑（2*radius）的倍數，不是絕對的 cellSize 數值，
// 這樣掃描結果才能在不同粒子大小的 scenario 之間比較。
const std::vector<float> kCellSizeRatios = {1.0f, 1.5f, 2.0f, 3.0f, 5.0f, 8.0f};

// ---------------------------------------------------------------------------
// Part B 用的常數：K x cellSizeRatio 完整格點掃描（hasSkin=true）。
//
// Part A 只在 hasSkin=false、K=0 的條件下找「結構最佳值」，隱含假設這個最佳值
// 開 skin 之後仍然適用。但 skin 大小本身跟 K 成正比，會直接影響 candidate
// 膨脹程度與 rebuild 節奏，兩者的最佳平衡點很可能隨 K 改變。Grid 只有
// cellSizeRatio 這一個結構維度，掃描成本低，所以直接對每個 K 做完整格點掃描。
//
// kSkinTotalFrames 沿用 phase2_k_sweep.cpp 的 1000（見該檔案註解：totalFrames
// 必須明顯大於最大 K 值才能真的看到多次 rebuild）——Part A 是 hasSkin=false，
// rebuild 幾乎每幀都發生，300 幀就夠看出穩態表現；Part B 要觀察 skin 帶來的
// rebuild 節奏，幀數太少會讓大 K 的 rebuild 次數太少、計時雜訊蓋過真正訊號。
constexpr int kSkinTotalFrames = 1000;
const std::vector<float> kKValues = {0.0f, 1.0f, 2.0f, 5.0f, 10.0f, 20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f};

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
    float cellSizeRatio = 0.0f;
    double avgTotalTimeS = 0.0;
};

}  // namespace

int main() {
    using namespace bench_runner;

    const std::vector<CsvColumn> columns = {
        {"scenario", 14, true},
        {"cell_size_ratio", 16, false},
        {"cell_size_actual", 17, false},
        {"avg_total_time_s", 16, false},
        {"total_time_std_s", 16, false},
        {"avg_broad_phase_time_s", 22, false},
        {"avg_narrow_phase_time_s", 23, false},
        {"avg_candidates_per_frame", 24, false},
        {"correctness_ok", 14, false},
        {"repeat_count", 12, false},
    };

    std::ofstream csv("phase1_grid_summary.csv");
    writeCsvHeader(csv, columns);

    BruteForceCache bfCache;

    // ---------------------------------------------------------------------------
    // Part A：hasSkin=false 的結構參數 baseline 掃描（原本就有的邏輯，不動）。
    // ---------------------------------------------------------------------------

    const size_t totalCombos = kScenarios.size() * kCellSizeRatios.size();
    size_t comboIndex = 0;

    std::unordered_map<std::string, BestCombo> bestPerScenario;
    for (const auto& s : kScenarios) bestPerScenario[s.name] = BestCombo();

    for (const auto& spec : kScenarios) {
        std::vector<Particle> particles = buildScenario(spec);

        for (float cellSizeRatio : kCellSizeRatios) {
            ++comboIndex;

            auto t0 = std::chrono::steady_clock::now();

            const float cellSizeActual = cellSizeRatio * (2.0f * kRadius);

            SimulationConfig cfg(
                kDt, /*K=*/0.0f, /*hasSkin=*/false, Method::UniformGrid,
                /*cellSize=*/cellSizeActual, /*maxDepth=*/8, /*leafCapacity=*/8, kBoxSize
            );

            RunResult perf = runAndAverage(particles, cfg, kTotalFrames, kRepeatCount);
            // 計時區塊到這裡結束，正確性驗證獨立於計時之外進行（見 bench_runner.h）。
            CorrectnessCheck check =
                verifyAgainstBruteForce(particles, cfg, kTotalFrames, bfCache, spec.name, spec.seed);

            auto t1 = std::chrono::steady_clock::now();
            double comboElapsedS = std::chrono::duration<double>(t1 - t0).count();

            if (!check.allMatch) {
                std::cerr << "[WARN] correctness mismatch: scenario=" << spec.name
                          << " cellSizeRatio=" << cellSizeRatio
                          << " firstMismatchFrame=" << check.firstMismatchFrame << "\n";
            }

            std::vector<std::string> row = {
                spec.name,
                formatFixed(cellSizeRatio, 2),
                formatFixed(cellSizeActual, 4),
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
                best.cellSizeRatio = cellSizeRatio;
                best.avgTotalTimeS = perf.totalTimeS;
            }

            std::cerr << "[" << comboIndex << "/" << totalCombos << "] "
                      << "scenario=" << spec.name << " cellSizeRatio=" << cellSizeRatio
                      << " cellSizeActual=" << formatFixed(cellSizeActual, 4)
                      << " avg_total_time_s=" << formatFixed(perf.totalTimeS, 6)
                      << " correctness_ok=" << (check.allMatch ? 1 : 0)
                      << " elapsed=" << formatFixed(comboElapsedS, 2) << "s\n";
        }
    }

    csv.close();

    std::cerr << "\n=== phase1_structural_grid: best cellSizeRatio by avg_total_time_s (hasSkin=false baseline) ===\n";
    for (const auto& spec : kScenarios) {
        const BestCombo& best = bestPerScenario[spec.name];
        std::cerr << spec.name << ": cellSizeRatio=" << formatFixed(best.cellSizeRatio, 2)
                   << " avg_total_time_s=" << formatFixed(best.avgTotalTimeS, 6) << "\n";
    }

    // ---------------------------------------------------------------------------
    // Part B：K x cellSizeRatio 完整格點掃描（hasSkin=true）。
    // 目的是取得「K -> 最佳 cellSizeRatio」這條關係，而不是沿用 Part A 那個
    // hasSkin=false 底下算出來的單一值。
    // ---------------------------------------------------------------------------

    std::cerr << "\n=== Part B: uniform_grid + skin，K x cellSizeRatio 完整格點掃描 ===\n";

    const std::vector<CsvColumn> skinColumns = {
        {"scenario", 14, true},
        {"K", 8, false},
        {"cell_size_ratio", 16, false},
        {"cell_size_actual", 17, false},
        {"avg_total_time_s", 16, false},
        {"total_time_std_s", 16, false},
        {"avg_broad_phase_time_s", 22, false},
        {"avg_narrow_phase_time_s", 23, false},
        {"avg_candidates_per_frame", 24, false},
        {"rebuild_count", 14, false},
        {"correctness_ok", 14, false},
        {"repeat_count", 12, false},
    };

    std::ofstream skinCsv("phase1_grid_skin_sweep.csv");
    writeCsvHeader(skinCsv, skinColumns);

    // 每個 scenario、每個 K 底下的最佳 cellSizeRatio —— 這才是該套進 phase2 的
    // 校準值（K 依賴），不是 Part A 那個跟 K 無關的單一值。
    std::unordered_map<std::string, std::unordered_map<float, BestCombo>> bestByScenarioAndK;

    const size_t totalSkinCombos = kScenarios.size() * kKValues.size() * kCellSizeRatios.size();
    size_t skinComboIndex = 0;

    for (const auto& spec : kScenarios) {
        std::vector<Particle> particles = buildScenario(spec);
        auto& bestByK = bestByScenarioAndK[spec.name];

        for (float K : kKValues) {
            BestCombo bestForK;

            for (float cellSizeRatio : kCellSizeRatios) {
                ++skinComboIndex;

                auto t0 = std::chrono::steady_clock::now();

                const float cellSizeActual = cellSizeRatio * (2.0f * kRadius);

                SimulationConfig cfg(
                    kDt, K, /*hasSkin=*/true, Method::UniformGrid,
                    /*cellSize=*/cellSizeActual, /*maxDepth=*/8, /*leafCapacity=*/8, kBoxSize
                );

                RunResult perf = runAndAverage(particles, cfg, kSkinTotalFrames, kRepeatCount);
                // 計時區塊到這裡結束，正確性驗證獨立於計時之外進行（見 bench_runner.h）。
                CorrectnessCheck check =
                    verifyAgainstBruteForce(particles, cfg, kSkinTotalFrames, bfCache, spec.name, spec.seed);

                auto t1 = std::chrono::steady_clock::now();
                double comboElapsedS = std::chrono::duration<double>(t1 - t0).count();

                if (!check.allMatch) {
                    std::cerr << "[WARN] correctness mismatch: scenario=" << spec.name << " K=" << K
                              << " cellSizeRatio=" << cellSizeRatio
                              << " firstMismatchFrame=" << check.firstMismatchFrame << "\n";
                }

                std::vector<std::string> row = {
                    spec.name,
                    formatFixed(K, 0),
                    formatFixed(cellSizeRatio, 2),
                    formatFixed(cellSizeActual, 4),
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
                    bestForK.cellSizeRatio = cellSizeRatio;
                    bestForK.avgTotalTimeS = perf.totalTimeS;
                }

                std::cerr << "[" << skinComboIndex << "/" << totalSkinCombos << "] "
                          << "scenario=" << spec.name << " K=" << K << " cellSizeRatio=" << cellSizeRatio
                          << " avg_total_time_s=" << formatFixed(perf.totalTimeS, 6)
                          << " rebuilds=" << perf.rebuildCount << " correctness_ok=" << (check.allMatch ? 1 : 0)
                          << " elapsed=" << formatFixed(comboElapsedS, 2) << "s\n";
            }

            bestByK[K] = bestForK;
        }
    }
    skinCsv.close();

    const std::vector<CsvColumn> bestByKColumns = {
        {"scenario", 14, true},
        {"K", 8, false},
        {"best_cell_size_ratio", 20, false},
        {"best_cell_size_actual", 21, false},
        {"avg_total_time_s", 16, false},
    };
    std::ofstream bestByKCsv("phase1_grid_best_by_k.csv");
    writeCsvHeader(bestByKCsv, bestByKColumns);

    std::cerr << "\n=== phase1_structural_grid: K -> 最佳 cellSizeRatio"
                 "（這條關係才是該套進 phase2 的校準值，不是單一固定值）===\n";
    for (const auto& spec : kScenarios) {
        const auto& bestByK = bestByScenarioAndK[spec.name];
        for (float K : kKValues) {
            const BestCombo& best = bestByK.at(K);
            const float bestCellSizeActual = best.cellSizeRatio * (2.0f * kRadius);

            writeCsvRow(bestByKCsv, bestByKColumns, {
                spec.name,
                formatFixed(K, 0),
                formatFixed(best.cellSizeRatio, 2),
                formatFixed(bestCellSizeActual, 4),
                formatFixed(best.avgTotalTimeS, 6),
            });

            std::cerr << spec.name << ": K=" << formatFixed(K, 0)
                       << " best_cellSizeRatio=" << formatFixed(best.cellSizeRatio, 2)
                       << " avg_total_time_s=" << formatFixed(best.avgTotalTimeS, 6) << "\n";
        }
    }
    bestByKCsv.close();

    return 0;
}
