// Reproduces the paper's parameter study (section 5.4 / Fig. 11-13): for
// each scenario, sweep the K factor and measure broad-phase time,
// narrow-phase time, and the fraction of skipped broad-phases.
// Runs the full scenario x K sweep once per SkinMode, writing each mode's
// results to its own CSV file:
//   bench_results_local_velocity.csv
//   bench_results_fixed_radius.csv
//   bench_results_none.csv

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

static void report(const std::string& scenario, const std::string& kLabel,
                    const BenchResult& r, int steps, std::ofstream& fout) {
    double skippedPct = 100.0 * (1.0 - (double)r.broadPhaseExecutions / steps);
    fout << scenario << "," << kLabel << "," << r.totalSeconds << ","
              << r.broadPhaseSeconds << "," << r.narrowPhaseSeconds << ","
              << r.broadPhaseExecutions << "," << skippedPct << "\n";
    std::cout << scenario << "," << kLabel << "," << r.totalSeconds << ","
              << r.broadPhaseSeconds << "," << r.narrowPhaseSeconds << ","
              << r.broadPhaseExecutions << "," << skippedPct << "\n";
}

struct NamedScenario {
    const char* name;
    scenario::Cloud cloud;
};

// One sweep pass per SkinMode: which mode to run, which file to write it to,
// and whether to append the paper's fixed-radius baseline row (Fig. 11's
// orange line) at the end of each scenario's block.
struct SweepMode {
    const char* fileSuffix;
    SimConfig::SkinMode mode;
    bool appendRadiusBaseline;
};

static void runSweep(const SweepMode& sweep, std::vector<NamedScenario>& scenarios,
                      const std::vector<int>& kValues, float cellSize, int steps) {
    std::string filename = std::string("bench_results_") + sweep.fileSuffix + ".csv";
    std::ofstream fout(filename);
    const char* header = "scenario,K,total_s,broadphase_s,narrowphase_s,broadphase_execs,skipped_pct\n";
    fout << header;
    std::cout << header;

    for (auto& sc : scenarios) {
        for (int K : kValues) {
            SimConfig cfg;
            cfg.K = (float)K;
            cfg.cellSize = cellSize;
            // K=0 makes the local-velocity skin identically zero, so label
            // it None for that mode; the other modes don't depend on K.
            cfg.skinMode = (sweep.mode == SimConfig::SkinMode::LocalVelocity && K == 0)
                               ? SimConfig::SkinMode::None
                               : sweep.mode;

            auto cloud = sc.cloud; // fresh copy so every K starts from the same state
            BenchResult r = runBench(cloud, cfg, steps);
            report(sc.name, std::to_string(K), r, steps, fout);
        }

        if (sweep.appendRadiusBaseline) {
            // Fixed skin = particle radius baseline (paper's orange line in Fig. 11).
            SimConfig cfgFixed;
            cfgFixed.cellSize = cellSize;
            cfgFixed.skinMode = SimConfig::SkinMode::FixedRadius;
            auto cloud = sc.cloud;
            BenchResult r = runBench(cloud, cfgFixed, steps);
            report(sc.name, "radius", r, steps, fout);
        }
    }
    fout.close();
}

int main() {
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

    const std::vector<SweepMode> sweeps = {
        {"local_velocity", SimConfig::SkinMode::LocalVelocity, /*appendRadiusBaseline=*/true},
        {"fixed_radius", SimConfig::SkinMode::FixedRadius, /*appendRadiusBaseline=*/true},
        {"none", SimConfig::SkinMode::None, /*appendRadiusBaseline=*/false},
    };

    for (const auto& sweep : sweeps) {
        runSweep(sweep, scenarios, kValues, cellSize, steps);
    }

    return 0;
}
