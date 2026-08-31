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

    const size_t totalCombos = kScenarios.size() * kCellSizeRatios.size();
    size_t comboIndex = 0;

    std::unordered_map<std::string, BestCombo> bestPerScenario;
    for (const auto& s : kScenarios) bestPerScenario[s.name] = BestCombo();

    for (const auto& spec : kScenarios) {
        std::vector<Particle> particles = buildScenario(spec);

        for (float cellSizeRatio : kCellSizeRatios) {
            ++comboIndex;

            auto t0 = std::chrono::high_resolution_clock::now();

            const float cellSizeActual = cellSizeRatio * (2.0f * kRadius);

            SimulationConfig cfg(
                kDt, /*K=*/0.0f, /*hasSkin=*/false, Method::UniformGrid,
                /*cellSize=*/cellSizeActual, /*maxDepth=*/8, /*leafCapacity=*/8, kBoxSize
            );

            RunResult perf = runAndAverage(particles, cfg, kTotalFrames, kRepeatCount);
            // 計時區塊到這裡結束，正確性驗證獨立於計時之外進行（見 bench_runner.h）。
            CorrectnessCheck check =
                verifyAgainstBruteForce(particles, cfg, kTotalFrames, bfCache, spec.name, spec.seed);

            auto t1 = std::chrono::high_resolution_clock::now();
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

    std::cerr << "\n=== phase1_structural_grid: best cellSizeRatio by avg_total_time_s ===\n";
    for (const auto& spec : kScenarios) {
        const BestCombo& best = bestPerScenario[spec.name];
        std::cerr << spec.name << ": cellSizeRatio=" << formatFixed(best.cellSizeRatio, 2)
                   << " avg_total_time_s=" << formatFixed(best.avgTotalTimeS, 6) << "\n";
    }

    return 0;
}
