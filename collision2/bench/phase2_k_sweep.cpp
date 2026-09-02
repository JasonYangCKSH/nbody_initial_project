// phase2_k_sweep.cpp — 在「已知最佳結構參數」（見下方三個具名常數）的前提下，
// 掃描 Verlet skin 的 K 值，觀察 rebuild 節奏、broad/narrow-phase 時間 trade-off，
// 並驗證 skin 版本的正確性：即使 K 很大、rebuild 很少，narrow-phase 撈出的碰撞對
// 還是要跟 brute force 完全一致。

#include "bench_runner.h"
#include "simulation.h"
#include "scenario.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// 校準後的結構參數 —— 來源是 phase1 的輸出結果，這裡只是抄一份定值進來，
// phase1 重跑出新結果後要記得回來同步更新。
// ---------------------------------------------------------------------------
constexpr int kOctreeLeafCapacity = 16;      // TODO: 依 phase1_octree_summary.csv 更新
constexpr int kOctreeMaxDepth = 8;          // TODO: 依 phase1_octree_summary.csv 更新
constexpr float kGridCellSizeRatio = 1.5f;  // TODO: 依 phase1_grid_summary.csv 更新（乘上 2*radius 得實際 cellSize）

namespace {

// 固定參數（見檔案開頭需求說明）
constexpr float kDt = 1.0f / 60.0f;
constexpr float kBoxSize = 60.0f;
constexpr int kParticleNum = 2000;
constexpr float kRadius = 1.0f;
constexpr float kSpeed = 1.5f;
constexpr float kAcc = 0.0f;
constexpr int kTotalFrames = 1000;  // 必須明顯大於最大的 K 值（1000），才能真的看到多次 rebuild
constexpr int kRepeatCount = 5;

const std::vector<float> kKValues = {0.0f, 1.0f, 2.0f, 5.0f, 10.0f, 20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f};

struct ScenarioSpec {
    std::string name;
    unsigned seed;
};

const std::vector<ScenarioSpec> kScenarios = {
    {"uniform_cloud", 100},
    //{"explosion", 100},
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

// K 欄位對「不受 K 影響」的 baseline 方法（brute_force / uniform_grid / octree，
// 皆 hasSkin=false）沒有意義 —— 用這個 sentinel 跟真正掃描到的 K>=0 區分開，
// CSV 裡印成 "NA"，方便後續分析時用 K>=0 篩出真正的 K 掃描資料。
constexpr float kNotApplicableK = -1.0f;

std::string formatK(float k) {
    if (k == kNotApplicableK) return "NA";
    return std::to_string(static_cast<long long>(k));
}

// 跑一次模擬並回傳逐幀歷史，專門給 bench_frames.csv 用。bench_runner::runOnce()/
// runAndAverage() 只回傳彙總過的 RunResult，沒有逐幀資料可拿。這個模擬是
// detection-only、完全 deterministic（見 bench_runner.h 對 BruteForceCache 的說明），
// 同一組 particles + cfg 不管重複跑幾次都會得到一模一樣的逐幀結果，所以另外單獨
// 跑一次來取得「這組 repeatCount 裡最後一次 repeat」的逐幀資料是安全的，不會跟
// runAndAverage() 內部真正計時用的那些 repeat 產生落差。這次額外的跑動本身不計時，
// 不會污染 bench_summary.csv 裡的效能數字。
std::vector<FrameStats> runForFrameHistory(const std::vector<Particle>& particles, const SimulationConfig& cfg, int totalFrames) {
    std::vector<Particle> particlesCopy(particles);
    Simulation sim(std::move(particlesCopy), cfg, totalFrames);
    return sim.run();
}

}  // namespace

int main() {
    using namespace bench_runner;

    const std::vector<CsvColumn> frameColumns = {
        {"scenario", 14, true},
        {"K", 8, false},
        {"structure_mode", 20, true},
        {"frame", 8, false},
        {"broad_phase_pairs", 18, false},
        {"collisions_pairs", 17, false},
        {"rebuilt", 8, false},
    };

    const std::vector<CsvColumn> summaryColumns = {
        {"scenario", 14, true},
        {"K", 8, false},
        {"structure_mode", 20, true},
        {"total_time_s", 14, false},
        {"total_time_std_s", 16, false},
        {"broad_phase_time_s", 18, false},
        {"narrow_phase_time_s", 19, false},
        {"other_time_s", 14, false},
        {"rebuild_count", 14, false},
        {"avg_candidates_per_frame", 24, false},
        {"correctness_ok", 14, false},
        {"repeat_count", 12, false},
    };

    std::ofstream framesCsv("bench_frames.csv");
    std::ofstream summaryCsv("bench_summary.csv");
    writeCsvHeader(framesCsv, frameColumns);
    writeCsvHeader(summaryCsv, summaryColumns);

    // 同一個 (scenario, seed, dt, totalFrames) 只會真的算一次 brute force ground
    // truth，後面所有 K / structure_mode 都是 cache hit，見 bench_runner.h。
    BruteForceCache bfCache;

    const size_t totalCombos = kScenarios.size() * 3                        // baseline: brute_force / uniform_grid / octree
                                + kScenarios.size() * 2 * kKValues.size();   // K 掃描: uniform_grid_skin / octree_skin
    size_t comboIndex = 0;

    // 共用的「跑一組 (scenario, K, structure_mode) 組合」流程：計時 → 正確性驗證
    // （在計時區塊之外）→ 取逐幀歷史（同樣在計時區塊之外）→ 寫兩份 CSV。
    auto runCombo = [&](const ScenarioSpec& spec, const std::vector<Particle>& particles,
                         const std::string& structureMode, float kForCsv, const SimulationConfig& cfg) {
        ++comboIndex;
        auto t0 = std::chrono::steady_clock::now();

        RunResult perf = runAndAverage(particles, cfg, kTotalFrames, kRepeatCount);
        // 計時區塊到這裡結束，正確性驗證與逐幀資料擷取都獨立於計時之外進行。
        CorrectnessCheck check =
            verifyAgainstBruteForce(particles, cfg, kTotalFrames, bfCache, spec.name, spec.seed);
        std::vector<FrameStats> history = runForFrameHistory(particles, cfg, kTotalFrames);

        auto t1 = std::chrono::steady_clock::now();
        double comboElapsedS = std::chrono::duration<double>(t1 - t0).count();

        if (!check.allMatch) {
            std::cerr << "[WARN] correctness mismatch: scenario=" << spec.name
                      << " structure_mode=" << structureMode << " K=" << formatK(kForCsv)
                      << " firstMismatchFrame=" << check.firstMismatchFrame << "\n";
        }

        std::vector<std::string> summaryRow = {
            spec.name,
            formatK(kForCsv),
            structureMode,
            formatFixed(perf.totalTimeS, 6),
            formatFixed(perf.totalTimeStdS, 6),
            formatFixed(perf.broadPhaseTimeS, 6),
            formatFixed(perf.narrowPhaseTimeS, 6),
            formatFixed(perf.otherTimeS, 6),
            std::to_string(perf.rebuildCount),
            formatFixed(perf.avgCandidatesPerFrame, 2),
            check.allMatch ? "1" : "0",
            std::to_string(kRepeatCount),
        };
        writeCsvRow(summaryCsv, summaryColumns, summaryRow);

        for (const auto& f : history) {
            std::vector<std::string> frameRow = {
                spec.name,
                formatK(kForCsv),
                structureMode,
                std::to_string(f.frameIndex),
                std::to_string(f.candidateCount()),
                std::to_string(f.collisionCount()),
                f.didRebuild ? "1" : "0",
            };
            writeCsvRow(framesCsv, frameColumns, frameRow);
        }

        std::cerr << "[" << comboIndex << "/" << totalCombos << "] "
                  << "scenario=" << spec.name << " structure_mode=" << structureMode
                  << " K=" << formatK(kForCsv)
                  << " total_time_s=" << formatFixed(perf.totalTimeS, 6)
                  << " rebuild_count=" << perf.rebuildCount
                  << " correctness_ok=" << (check.allMatch ? 1 : 0)
                  << " elapsed=" << formatFixed(comboElapsedS, 2) << "s\n";
    };

    for (const auto& spec : kScenarios) {
        std::vector<Particle> particles = buildScenario(spec);
        const float gridCellSize = kGridCellSizeRatio * 2.0f * kRadius;

        // --- baseline：不受 K 影響（hasSkin=false），每個 scenario 只跑一次 ---
        SimulationConfig bruteCfg(
            kDt, /*K=*/0.0f, /*hasSkin=*/false, Method::BruteForce,
            /*cellSize=*/1.0f, /*maxDepth=*/8, /*leafCapacity=*/8, kBoxSize
        );
        runCombo(spec, particles, "brute_force", kNotApplicableK, bruteCfg);

        SimulationConfig gridCfg(
            kDt, /*K=*/0.0f, /*hasSkin=*/false, Method::UniformGrid,
            gridCellSize, /*maxDepth=*/8, /*leafCapacity=*/8, kBoxSize
        );
        runCombo(spec, particles, "uniform_grid", kNotApplicableK, gridCfg);

        SimulationConfig octreeCfg(
            kDt, /*K=*/0.0f, /*hasSkin=*/false, Method::Octree,
            /*cellSize=*/1.0f, kOctreeMaxDepth, kOctreeLeafCapacity, kBoxSize
        );
        runCombo(spec, particles, "octree", kNotApplicableK, octreeCfg);

        // --- K 掃描：只有 uniform_grid+skin 與 octree+skin ---
        for (float k : kKValues) {
            SimulationConfig gridSkinCfg(
                kDt, k, /*hasSkin=*/true, Method::UniformGrid,
                gridCellSize, /*maxDepth=*/8, /*leafCapacity=*/8, kBoxSize
            );
            runCombo(spec, particles, "uniform_grid_skin", k, gridSkinCfg);

            SimulationConfig octreeSkinCfg(
                kDt, k, /*hasSkin=*/true, Method::Octree,
                /*cellSize=*/1.0f, kOctreeMaxDepth, kOctreeLeafCapacity, kBoxSize
            );
            runCombo(spec, particles, "octree_skin", k, octreeSkinCfg);
        }
    }

    framesCsv.close();
    summaryCsv.close();

    std::cerr << "\n=== phase2_k_sweep: done (" << totalCombos << " combos) ===\n";
    return 0;
}
