// bench.cpp — 第二版效能掃描工具
//
// 固定不掃描（用合理預設值，見 BenchDefaults）：particleNum, dt, cellSize,
// leafCapacity, maxDepth, boxSize, totalFrames, radius, speed, acc。
//
// 主要掃描變因：
//   K        : {0, 1, 2, 5, 10, 20, 50, 100, 200, 500, 1000}
//   scenario : {uniform_cloud, explosion}
//   structure_mode（5 組）: brute_force, uniform_grid, uniform_grid+skin,
//                           octree, octree+skin
//
// 同一個 scenario 用固定 seed（跟 K、structure_mode 無關），確保同一組 K
// 底下 5 種 structure_mode 是在同一組初始條件上跑，才能公平比較彼此的
// broad-phase 效率；K 的變化只影響 skin 版本的 rebuild 節奏。
//
// 每個 (scenario, K, structure_mode) 組合都完整跑 totalFrames 幀：
//   - 過程中每一幀輸出一列到 bench_frames.csv：broad_phase_pairs /
//     collisions_pairs 直接是 FrameStats::candidateCount() /
//     collisionCount()（本來就是 size_t，天然整數，不做平均）。
//   - 整個組合跑完 totalFrames 幀後，輸出一列到 bench_summary.csv：
//     total_time_s / broad_phase_time_s / narrow_phase_time_s /
//     other_time_s / rebuild_count。
//
// 兩種粒度（逐幀 vs. 整個 run 的彙總）schema 不同，所以分成兩個檔案，
// 而不是混在同一個 CSV 裡。進度訊息輸出到 stderr。

#include "simulation.h"
#include "scenario.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

struct BenchDefaults {
    int particleNum = 2000;
    float dt = 1.0f / 60.0f;
    float cellSize = 3.0f;
    int leafCapacity = 8;
    int maxDepth = 8;
    float boxSize = 60.0f;
    int totalFrames = 40;

    // 場景產生器參數（不在掃描變因裡，但同樣需要固定值）。
    float radius = 1.0f;
    float speed = 1.5f;
    float acc = 0.0f;  // 不開隨機加速度，避免多一個變因干擾 K 掃描的解讀
};

struct StructureMode {
    std::string name;
    Method method;
    bool hasSkin;
};

struct SummaryRow {
    std::string scenario;
    int K = 0;
    std::string structureMode;
    double totalTimeS = 0.0;
    double broadPhaseTimeS = 0.0;
    double narrowPhaseTimeS = 0.0;
    double otherTimeS = 0.0;
    int rebuildCount = 0;
};

std::vector<Particle> makeScenario(const std::string& scenario, const BenchDefaults& d, unsigned seed) {
    if (scenario == "uniform_cloud") {
        return scenario::uniformCloud(d.particleNum, d.boxSize, d.radius, d.speed, d.acc, seed);
    }
    return scenario::explosion(d.particleNum, d.boxSize, d.radius, d.speed, seed);
}

// 每個 scenario 用固定 seed，跟 K / structure_mode 無關，確保同一組 K 底下
// 5 種 structure_mode 看到完全相同的初始粒子分佈。
unsigned seedForScenario(const std::string& scenario) { return scenario == "uniform_cloud" ? 100u : 200u; }

// 欄寬常數：讓 header 跟每一列資料對齊，方便直接用文字編輯器看，不用另外開 Excel。
constexpr int kScenarioW = 15;
constexpr int kModeW = 20;
constexpr int kKW = 6;
constexpr int kFrameW = 7;
constexpr int kPairsW = 20;
constexpr int kRebuiltW = 9;
constexpr int kTimeW = 20;
constexpr int kRebuildCountW = 15;

void writeFrameHeader(std::ostream& os) {
    os << std::left << std::setw(kScenarioW) << "scenario" << ',' << std::right << std::setw(kKW) << "K" << ','
       << std::left << std::setw(kModeW) << "structure_mode" << ',' << std::right << std::setw(kFrameW) << "frame"
       << ',' << std::right << std::setw(kPairsW) << "broad_phase_pairs" << ',' << std::right
       << std::setw(kPairsW) << "collisions_pairs" << ',' << std::right << std::setw(kRebuiltW) << "rebuilt"
       << '\n';
}

void writeSummaryHeader(std::ostream& os) {
    os << std::left << std::setw(kScenarioW) << "scenario" << ',' << std::right << std::setw(kKW) << "K" << ','
       << std::left << std::setw(kModeW) << "structure_mode" << ',' << std::right << std::setw(kTimeW)
       << "total_time_s" << ',' << std::right << std::setw(kTimeW) << "broad_phase_time_s" << ',' << std::right
       << std::setw(kTimeW) << "narrow_phase_time_s" << ',' << std::right << std::setw(kTimeW) << "other_time_s"
       << ',' << std::right << std::setw(kRebuildCountW) << "rebuild_count" << '\n';
}

// 跑完一個 (scenario, K, structure_mode) 組合的 totalFrames 幀：逐幀把整數
// broad_phase_pairs / collisions_pairs 寫進 framesOut，最後把整個 run 的計時
// 彙總寫進 summaryOut。
void runOne(const std::string& scenarioName, int K, const StructureMode& mode, const BenchDefaults& d,
            std::ostream& framesOut, std::ostream& summaryOut) {
    std::vector<Particle> particles = makeScenario(scenarioName, d, seedForScenario(scenarioName));

    SimulationConfig cfg(d.dt, static_cast<float>(K), mode.hasSkin, mode.method, d.cellSize, d.maxDepth,
                          d.leafCapacity, d.boxSize);
    Simulation sim(particles, cfg, d.totalFrames);

    auto t0 = std::chrono::high_resolution_clock::now();
    std::vector<FrameStats> history = sim.run();
    auto t1 = std::chrono::high_resolution_clock::now();

    double broadMs = 0.0, narrowMs = 0.0;
    int rebuilds = 0;
    for (const auto& f : history) {
        broadMs += f.broadPhaseTimeMs;
        narrowMs += f.narrowPhaseTimeMs;
        if (f.didRebuild) rebuilds++;

        // 逐幀整數輸出：candidateCount()/collisionCount() 本來就是 size_t，
        // 這裡是「該幀底下找到幾對粒子對」的原始計數，不做平均。
        framesOut << std::left << std::setw(kScenarioW) << scenarioName << ',' << std::right << std::setw(kKW) << K
                   << ',' << std::left << std::setw(kModeW) << mode.name << ',' << std::right << std::setw(kFrameW)
                   << f.frameIndex << ',' << std::right << std::setw(kPairsW) << f.candidateCount() << ','
                   << std::right << std::setw(kPairsW) << f.collisionCount() << ',' << std::right
                   << std::setw(kRebuiltW) << (f.didRebuild ? 1 : 0) << '\n';
    }

    SummaryRow row;
    row.scenario = scenarioName;
    row.K = K;
    row.structureMode = mode.name;
    row.totalTimeS = std::chrono::duration<double>(t1 - t0).count();
    row.broadPhaseTimeS = broadMs / 1000.0;
    row.narrowPhaseTimeS = narrowMs / 1000.0;
    row.otherTimeS = row.totalTimeS - row.broadPhaseTimeS - row.narrowPhaseTimeS;
    row.rebuildCount = rebuilds;

    summaryOut << std::left << std::setw(kScenarioW) << row.scenario << ',' << std::right << std::setw(kKW)
               << row.K << ',' << std::left << std::setw(kModeW) << row.structureMode << ',' << std::right
               << std::fixed << std::setprecision(6) << std::setw(kTimeW) << row.totalTimeS << ',' << std::right
               << std::setw(kTimeW) << row.broadPhaseTimeS << ',' << std::right << std::setw(kTimeW)
               << row.narrowPhaseTimeS << ',' << std::right << std::setw(kTimeW) << row.otherTimeS << ','
               << std::right << std::setw(kRebuildCountW) << row.rebuildCount << '\n';
}

}  // namespace

int main() {
    BenchDefaults d;

    const std::vector<int> kValues = {0, 1, 2, 5, 10, 20, 50, 100, 200, 500, 1000};
    const std::vector<std::string> scenarios = {"uniform_cloud", "explosion"};
    const std::vector<StructureMode> modes = {
        {"brute_force", Method::BruteForce, false},   {"uniform_grid", Method::UniformGrid, false},
        {"uniform_grid+skin", Method::UniformGrid, true}, {"octree", Method::Octree, false},
        {"octree+skin", Method::Octree, true},
    };

    std::ofstream framesOut("bench_frames.csv");
    std::ofstream summaryOut("bench_summary.csv");
    if (!framesOut || !summaryOut) {
        std::cerr << "無法開啟 bench_frames.csv / bench_summary.csv\n";
        return 1;
    }
    writeFrameHeader(framesOut);
    writeSummaryHeader(summaryOut);

    const size_t totalRuns = scenarios.size() * kValues.size() * modes.size();
    size_t doneRuns = 0;

    for (const auto& scenarioName : scenarios) {
        for (int K : kValues) {
            for (const auto& mode : modes) {
                auto t0 = std::chrono::high_resolution_clock::now();
                runOne(scenarioName, K, mode, d, framesOut, summaryOut);
                auto t1 = std::chrono::high_resolution_clock::now();
                framesOut.flush();
                summaryOut.flush();

                ++doneRuns;
                std::cerr << "[" << doneRuns << "/" << totalRuns << "] " << scenarioName << " K=" << K << " "
                          << mode.name << " -> " << std::fixed << std::setprecision(3)
                          << std::chrono::duration<double>(t1 - t0).count() << "s\n";
            }
        }
        
    }

    std::cerr << "done: bench_frames.csv (" << (totalRuns * static_cast<size_t>(d.totalFrames))
              << " rows), bench_summary.csv (" << totalRuns << " rows)\n";
    return 0;
}
