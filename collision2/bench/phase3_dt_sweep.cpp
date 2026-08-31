// phase3_dt_sweep.cpp — 在固定 K 值（kFixedK）之下掃描時間步長 dt，觀察 dt 如何透過
// skin 公式 skin_p = K*v_p*dt + 0.5*a_p*(K*dt)^2 與 rebuild 判斷條件 Δx_p ≤ skin_p，
// 影響 broad/narrow-phase 時間的 trade-off，以及固定 K 值下的實際 rebuild cadence。
//
// 這個實驗跟畫面流暢度無關：模擬是 detection-only（不做碰撞回應以外的渲染輸出），
// dt 在這裡純粹是 (1) skin 公式裡的乘數 —— dt 越大同樣 K 值產生的 skin margin 越大；
// (2) rebuild 判斷條件的檢查頻率 —— dt 太大會讓單步位移容易超過 skin，導致 rebuild
// 過於頻繁、skin 機制效益打折。
//
// 固定 totalPhysicalTime 不變，每個 dt 對應的 totalFrames 用
// totalFrames = round(totalPhysicalTime / dt) 動態算出來，讓不同 dt 之間比較的
// 都是同一段物理時間，差異只來自取樣密度跟 skin 公式裡的 dt 項。

#include "bench_runner.h"
#include "simulation.h"
#include "scenario.h"

#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// 校準後的結構參數 —— 來源是 phase1 的輸出結果，這裡只是抄一份定值進來，
// phase1 重跑出新結果後要記得回來同步更新。
// ---------------------------------------------------------------------------
constexpr int kOctreeLeafCapacity = 8;      // TODO: 依 phase1_octree_summary.csv 更新
constexpr int kOctreeMaxDepth = 8;          // TODO: 依 phase1_grid_summary.csv 更新
constexpr float kGridCellSizeRatio = 2.0f;  // TODO: 依 phase1_grid_summary.csv 更新（乘上 2*radius 得實際 cellSize）

namespace {

// 固定參數（見檔案開頭需求說明）
constexpr float kBoxSize = 60.0f;
constexpr int kParticleNum = 2000;
constexpr float kRadius = 1.0f;
constexpr float kSpeed = 1.5f;
constexpr float kAcc = 0.0f;
constexpr int kRepeatCount = 5;
constexpr float kTotalPhysicalTime = 50.0f;  // 秒，所有 dt 都對應到同一段物理時間

// 固定一個代表性 K 值：依論文 Checkaraou et al. 2022 Section 5.4 的建議，這是各
// test case 中普遍接近最佳值的折衷選擇；可依 phase2_k_sweep 的實際掃描結果調整。
constexpr int kFixedK = 200;

const std::vector<float> kDtValues = {1.0f / 30.0f, 1.0f / 60.0f, 1.0f / 120.0f, 1.0f / 240.0f};

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

int derivedTotalFrames(float dt) {
    return static_cast<int>(std::round(kTotalPhysicalTime / dt));
}

}  // namespace

int main() {
    using namespace bench_runner;

    const std::vector<CsvColumn> summaryColumns = {
        {"scenario", 14, true},
        {"structure_mode", 18, true},
        {"dt", 12, false},
        {"total_frames_derived", 22, false},
        {"total_time_s", 14, false},
        {"total_time_std_s", 16, false},
        {"broad_phase_time_s", 18, false},
        {"narrow_phase_time_s", 19, false},
        {"rebuild_count", 14, false},
        {"rebuild_count_per_physical_second", 34, false},
        {"avg_candidates_per_frame", 24, false},
        {"correctness_ok", 14, false},
        {"repeat_count", 12, false},
    };

    std::ofstream summaryCsv("phase3_dt_summary.csv");
    writeCsvHeader(summaryCsv, summaryColumns);

    // 同一個 (scenario, seed, dt, totalFrames) 只會真的算一次 brute force ground
    // truth；不同 dt 的 totalFrames 也不同，所以每個 dt 都會各自觸發一次 brute force
    // 計算 —— 這是預期行為，因為 dt 不同物理軌跡的取樣點也不同（見 bench_runner.h）。
    BruteForceCache bfCache;

    const size_t totalCombos = kScenarios.size() * 2 * kDtValues.size();  // structure_mode: uniform_grid_skin / octree_skin
    size_t comboIndex = 0;

    auto runCombo = [&](const ScenarioSpec& spec, const std::vector<Particle>& particles,
                         const std::string& structureMode, float dt, const SimulationConfig& cfg) {
        ++comboIndex;
        const int totalFrames = derivedTotalFrames(dt);
        auto t0 = std::chrono::high_resolution_clock::now();

        RunResult perf = runAndAverage(particles, cfg, totalFrames, kRepeatCount);
        // 計時區塊到這裡結束，正確性驗證獨立於計時之外進行。
        CorrectnessCheck check =
            verifyAgainstBruteForce(particles, cfg, totalFrames, bfCache, spec.name, spec.seed);

        auto t1 = std::chrono::high_resolution_clock::now();
        double comboElapsedS = std::chrono::duration<double>(t1 - t0).count();

        if (!check.allMatch) {
            std::cerr << "[WARN] correctness mismatch: scenario=" << spec.name
                      << " structure_mode=" << structureMode << " dt=" << formatFixed(dt, 6)
                      << " firstMismatchFrame=" << check.firstMismatchFrame << "\n";
        }

        const double rebuildPerPhysicalSecond = static_cast<double>(perf.rebuildCount) / kTotalPhysicalTime;

        std::vector<std::string> summaryRow = {
            spec.name,
            structureMode,
            formatFixed(dt, 6),
            std::to_string(totalFrames),
            formatFixed(perf.totalTimeS, 6),
            formatFixed(perf.totalTimeStdS, 6),
            formatFixed(perf.broadPhaseTimeS, 6),
            formatFixed(perf.narrowPhaseTimeS, 6),
            std::to_string(perf.rebuildCount),
            formatFixed(rebuildPerPhysicalSecond, 4),
            formatFixed(perf.avgCandidatesPerFrame, 2),
            check.allMatch ? "1" : "0",
            std::to_string(kRepeatCount),
        };
        writeCsvRow(summaryCsv, summaryColumns, summaryRow);

        std::cerr << "[" << comboIndex << "/" << totalCombos << "] "
                  << "scenario=" << spec.name << " structure_mode=" << structureMode
                  << " dt=" << formatFixed(dt, 6) << " total_frames=" << totalFrames
                  << " total_time_s=" << formatFixed(perf.totalTimeS, 6)
                  << " rebuild_count=" << perf.rebuildCount
                  << " rebuild_per_s=" << formatFixed(rebuildPerPhysicalSecond, 4)
                  << " correctness_ok=" << (check.allMatch ? 1 : 0)
                  << " elapsed=" << formatFixed(comboElapsedS, 2) << "s\n";
    };

    for (const auto& spec : kScenarios) {
        std::vector<Particle> particles = buildScenario(spec);
        const float gridCellSize = kGridCellSizeRatio * 2.0f * kRadius;

        for (float dt : kDtValues) {
            SimulationConfig gridSkinCfg(
                dt, static_cast<float>(kFixedK), /*hasSkin=*/true, Method::UniformGrid,
                gridCellSize, /*maxDepth=*/8, /*leafCapacity=*/8, kBoxSize
            );
            runCombo(spec, particles, "uniform_grid_skin", dt, gridSkinCfg);

            SimulationConfig octreeSkinCfg(
                dt, static_cast<float>(kFixedK), /*hasSkin=*/true, Method::Octree,
                /*cellSize=*/1.0f, kOctreeMaxDepth, kOctreeLeafCapacity, kBoxSize
            );
            runCombo(spec, particles, "octree_skin", dt, octreeSkinCfg);
        }
    }

    summaryCsv.close();

    std::cerr << "\n=== phase3_dt_sweep: done (" << totalCombos << " combos, K=" << kFixedK << ") ===\n";
    return 0;
}
