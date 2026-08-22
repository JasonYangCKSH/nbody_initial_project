// Reproduces the paper's parameter study (section 5.4 / Fig. 11-13): for
// each scenario, sweep the K factor and measure broad-phase time,
// narrow-phase time, and the fraction of skipped broad-phases.
// Runs the full scenario x K sweep once per SkinMode, writing each mode's
// results to its own CSV file:
//   bench_result_local_velocity.csv
//   bench_result_fixed_radius.csv
//   bench_result_none.csv

#include "../include/particle.h"
#include "../include/scenarios.h"
#include "../include/simulation.h"

#include <chrono>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>

struct BenchResult {
    double totalSeconds = 0.0;
    double broadPhaseSeconds = 0.0;
    double narrowPhaseSeconds = 0.0;
    int broadPhaseExecutions = 0;
};

static BenchResult runBench(scenario::Cloud particles, SimConfig cfg, int steps) {
    Simulation sim(cfg);
    BenchResult result;

    auto t0 = std::chrono::steady_clock::now();
    for (int s = 0; s < steps; ++s) {
        StepStats st = sim.step(particles, /*recordCollisions=*/false);
        result.broadPhaseSeconds += st.broadPhaseSeconds;
        result.narrowPhaseSeconds += st.narrowPhaseSeconds;
        result.broadPhaseExecutions += st.broadPhaseExecuted ? 1 : 0;
    }
    auto t1 = std::chrono::steady_clock::now();
    result.totalSeconds = std::chrono::duration<double>(t1 - t0).count();
    return result;
}

// 重複跑 repeats 次、取平均，壓低系統排程雜訊對計時結果的影響。
static BenchResult runBenchAveraged(const scenario::Cloud& baseCloud, const SimConfig& cfg,
                                     int steps, int repeats) {
    BenchResult sum;
    for (int r = 0; r < repeats; ++r) {
        auto cloud = baseCloud; // 每次重複都從乾淨狀態開始
        BenchResult res = runBench(cloud, cfg, steps);
        sum.totalSeconds += res.totalSeconds;
        sum.broadPhaseSeconds += res.broadPhaseSeconds;
        sum.narrowPhaseSeconds += res.narrowPhaseSeconds;
        sum.broadPhaseExecutions += res.broadPhaseExecutions;
    }
    BenchResult avg;
    avg.totalSeconds = sum.totalSeconds / repeats;
    avg.broadPhaseSeconds = sum.broadPhaseSeconds / repeats;
    avg.narrowPhaseSeconds = sum.narrowPhaseSeconds / repeats;
    avg.broadPhaseExecutions = (int)std::lround((double)sum.broadPhaseExecutions / repeats);
    return avg;
}

static void report(std::ostream& out, const std::string& scenario, const std::string& kLabel,
                    const BenchResult& r, int steps) {
    double skippedPct = 100.0 * (1.0 - (double)r.broadPhaseExecutions / steps);
    out << scenario << "," << kLabel << "," << r.totalSeconds << ","
        << r.broadPhaseSeconds << "," << r.narrowPhaseSeconds << ","
        << r.broadPhaseExecutions << "," << skippedPct << "\n";
}

struct NamedScenario {
    const char* name;
    scenario::Cloud cloud;
};

int main() {
    const float boxSize = 20.0f;
    const float radius = 0.1f;
    const float cellSize = 0.8f;
    const int steps = 500;
    const int repeats = 1; // 每個 (scenario, K) 點重複跑幾次取平均

    std::vector<NamedScenario> scenarios = {
        {"uniform_cloud", scenario::uniformCloud(2000, boxSize, radius, 0.3f)},
        {"free_fall", scenario::freeFall(2000, boxSize, radius)},
        {"mixed_regime", scenario::mixedRegime(1000, 1000, boxSize, radius, 0.05f, 2.0f)},
        {"explosion", scenario::explosion(2000, boxSize, radius, 3.0f)},
    };

    const std::vector<int> kValues = {0, 1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000};

    // 三種 mode，各自對應一個輸出檔案。
    struct ModeConfig {
        const char* filename;
        SimConfig::SkinMode mode;
    };
    std::vector<ModeConfig> modes = {
        {"bench_results_local_velocity.csv", SimConfig::SkinMode::LocalVelocity},
        {"bench_results_fixed_radius.csv", SimConfig::SkinMode::FixedRadius},
        {"bench_results_none.csv", SimConfig::SkinMode::None},
    };

    for (auto& modeCfg : modes) {
        std::ofstream fout(modeCfg.filename);
        if (!fout.is_open()) {
            std::cerr << "Failed to open " << modeCfg.filename << "\n";
            return 1;
        }
        const char* header = "scenario,K,total_s,broadphase_s,narrowphase_s,broadphase_execs,skipped_pct\n";
        fout << header;
        std::cout << "== " << modeCfg.filename << " ==\n" << header;

        for (auto& sc : scenarios) {
            for (int K : kValues) {
                SimConfig cfg;
                cfg.K = (float)K;
                cfg.cellSize = cellSize;
                cfg.skinMode = modeCfg.mode;

                BenchResult r = runBenchAveraged(sc.cloud, cfg, steps, repeats);
                report(fout, sc.name, std::to_string(K), r, steps);
                report(std::cout, sc.name, std::to_string(K), r, steps);
            }
        }

        fout.close();
        std::cout << "Results written to " << modeCfg.filename << "\n";
    }

    return 0;
}
