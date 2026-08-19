#include "../include/particle.h"
#include "../include/scenarios.h"
#include "../include/simulation.h"

#include <chrono>
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

// 改成寫進任意 ostream（可以是檔案，也可以是 std::cout）
static void report(std::ostream& out, const std::string& scenario,
                    const std::string& kLabel, const BenchResult& r, int steps) {
    double skippedPct = 100.0 * (1.0 - (double)r.broadPhaseExecutions / steps);
    out << scenario << "," << kLabel << "," << r.totalSeconds << ","
        << r.broadPhaseSeconds << "," << r.narrowPhaseSeconds << ","
        << r.broadPhaseExecutions << "," << skippedPct << "\n";
}

int main() {
    struct NamedScenario {
        const char* name;
        scenario::Cloud cloud;
    };

    const float boxSize = 20.0f;
    const float radius = 0.1f;
    const float cellSize = 0.8f;
    const int steps = 500;

    std::vector<NamedScenario> scenarios = {
        {"uniform_cloud", scenario::uniformCloud(2000, boxSize, radius, 0.3f)},
        {"free_fall", scenario::freeFall(2000, boxSize, radius)},
        {"mixed_regime", scenario::mixedRegime(1000, 1000, boxSize, radius, 0.05f, 2.0f)},
        {"explosion", scenario::explosion(2000, boxSize, radius, 3.0f)},
    };

    const std::vector<int> kValues = {0, 1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000};

    // 兩種模式，各自產生一份檔案
    struct ModeConfig {
        const char* filename;
        SimConfig::SkinMode mode;
    };
    std::vector<ModeConfig> modes = {
        {"bench_results_local_velocity.csv", SimConfig::SkinMode::LocalVelocity},
        {"bench_results_fixed_radius.csv", SimConfig::SkinMode::FixedRadius},
    };

    for (auto& modeCfg : modes) {
        std::ofstream outFile(modeCfg.filename);
        if (!outFile.is_open()) {
            std::cerr << "Failed to open " << modeCfg.filename << "\n";
            return 1;
        }
        outFile << "scenario,K,total_s,broadphase_s,narrowphase_s,broadphase_execs,skipped_pct\n";

        for (auto& sc : scenarios) {
            for (int K : kValues) {
                SimConfig cfg;
                cfg.K = (float)K;
                cfg.cellSize = cellSize;
                cfg.skinMode = (K == 0) ? SimConfig::SkinMode::None : modeCfg.mode;

                auto cloud = sc.cloud;
                BenchResult r = runBench(cloud, cfg, steps);
                report(outFile, sc.name, std::to_string(K), r, steps);
                
            }
            break;
        }
        outFile.close();
        std::cout << "Results written to " << modeCfg.filename << "\n";
    }

    return 0;
}