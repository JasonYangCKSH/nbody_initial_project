// phase4_scene_indices.cpp — 用連續數值指標（solid fraction、velocity CV）描述場景特性，
// 取代 phase2/phase3 那種 uniform_cloud / explosion 離散類別命名法，觀察「最佳 K 值」
// 如何隨場景密度與速度異質性連續變化，方便事後對這些連續指標做回歸分析。
//
// 這一版只掃兩個軸：
//   - particleNum（在固定 boxSize/radius 下決定 solid_fraction_actual，密度軸）
//   - speedCV（scenario::synthesizeScene() 的速度異質性參數，速度軸）
// 加速度異質性目前不掃描——scenario::synthesizeScene() 目前每軸獨立 uniform 取樣 acc，
// 還沒有獨立控制 acc 異質性的機制（skin_p 公式已經納入 a_p，等模型定案後再擴充這個軸），
// 但每個場景實際產生出來的 acc_magnitude_cv_actual 還是被動記錄進 CSV，供未來擴充時
// 對照歷史資料用。
//
// 只跑 octree_skin 這個 structure_mode：這個實驗的重點是「連續場景指標 vs. 最佳 K」的
// 關係，不是重新比較 structure_mode（那是 phase1/phase2 的範疇）。

#include "bench_runner.h"
#include "simulation.h"
#include "scenario.h"

#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// 校準後的結構參數 —— 沿用 phase2_k_sweep.cpp 開頭那組值，來源是 phase1 的輸出結果。
// phase1 重跑出新結果後要記得回來同步更新。
// ---------------------------------------------------------------------------
constexpr int kOctreeLeafCapacity = 8;  // TODO: 依 phase1_octree_summary.csv 更新
constexpr int kOctreeMaxDepth = 8;      // TODO: 依 phase1_octree_summary.csv 更新

namespace {

// 固定參數（見檔案開頭需求說明）
constexpr float kDt = 1.0f / 60.0f;
constexpr float kBoxSize = 60.0f;
constexpr float kRadius = 1.0f;
constexpr float kMeanSpeed = 1.5f;
constexpr float kAccMagnitude = 0.5f;  // 固定給一個小的非零值，避免退化成 0；這個 phase 不掃描
constexpr int kTotalFrames = 3000;     // 必須明顯大於最大的 K 值（1000），才能真的看到多次 rebuild
constexpr int kRepeatCount = 3;        // 組合數較多（25 場景 x 8 個 K），比 phase2 少一點控制總時間
constexpr unsigned kSceneSeedBase = 2000;

const std::vector<int> kParticleNums = {500, 1000, 2000, 4000, 8000};
const std::vector<float> kSpeedCVs = {0.1f, 0.3f, 0.5f, 0.8f, 1.2f};
const std::vector<float> kKValues = {0.0f, 5.0f, 20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f};

struct SceneSpec {
    std::string name;
    int particleNum;
    float speedCV;
    unsigned seed;
};

std::vector<SceneSpec> buildSceneSpecs() {
    std::vector<SceneSpec> specs;
    specs.reserve(kParticleNums.size() * kSpeedCVs.size());
    size_t index = 0;
    for (int particleNum : kParticleNums) {
        for (float speedCV : kSpeedCVs) {
            std::ostringstream nameOss;
            nameOss << "n" << particleNum << "_cv" << speedCV;
            specs.push_back(SceneSpec{nameOss.str(), particleNum, speedCV, kSceneSeedBase + static_cast<unsigned>(index)});
            ++index;
        }
    }
    return specs;
}

std::string formatFixed(double value, int precision) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << value;
    return oss.str();
}

// 場景實際指標：一律從產生出來的粒子資料算，不是憑空用輸入參數，因為隨機性可能讓
// 實際值跟參數設定值有落差。
struct SceneMetrics {
    double solidFractionActual = 0.0;
    double velocityCVActual = 0.0;
    double accMagnitudeCVActual = 0.0;
};

double meanOf(const std::vector<double>& values) {
    if (values.empty()) return 0.0;
    double sum = 0.0;
    for (double v : values) sum += v;
    return sum / static_cast<double>(values.size());
}

double stdOf(const std::vector<double>& values, double mean) {
    if (values.empty()) return 0.0;
    double variance = 0.0;
    for (double v : values) {
        const double diff = v - mean;
        variance += diff * diff;
    }
    variance /= static_cast<double>(values.size());
    return std::sqrt(variance);
}

SceneMetrics computeSceneMetrics(const std::vector<Particle>& particles, float boxSize) {
    SceneMetrics metrics;

    double solidVolumeSum = 0.0;
    std::vector<double> speedMags;
    std::vector<double> accMags;
    speedMags.reserve(particles.size());
    accMags.reserve(particles.size());

    for (const auto& p : particles) {
        const double r = static_cast<double>(p.radius);
        solidVolumeSum += (4.0 / 3.0) * M_PI * r * r * r;
        speedMags.push_back(static_cast<double>(glm::length(p.vel)));
        accMags.push_back(static_cast<double>(glm::length(p.acc)));
    }

    const double boxVolume = static_cast<double>(boxSize) * static_cast<double>(boxSize) * static_cast<double>(boxSize);
    metrics.solidFractionActual = solidVolumeSum / boxVolume;

    const double speedMean = meanOf(speedMags);
    const double speedStd = stdOf(speedMags, speedMean);
    metrics.velocityCVActual = (speedMean > 0.0) ? speedStd / speedMean : 0.0;

    const double accMean = meanOf(accMags);
    const double accStd = stdOf(accMags, accMean);
    metrics.accMagnitudeCVActual = (accMean > 0.0) ? accStd / accMean : 0.0;

    return metrics;
}

}  // namespace

int main() {
    using namespace bench_runner;

    const std::vector<CsvColumn> columns = {
        {"particle_num", 14, false},
        {"speed_cv_param", 16, false},
        {"solid_fraction_actual", 22, false},
        {"velocity_cv_actual", 20, false},
        {"acc_magnitude_cv_actual", 24, false},
        {"K", 8, false},
        {"total_time_s", 14, false},
        {"broad_phase_time_s", 18, false},
        {"narrow_phase_time_s", 19, false},
        {"rebuild_count", 14, false},
        {"avg_candidates_per_frame", 24, false},
        {"correctness_ok", 14, false},
        {"repeat_count", 12, false},
    };

    std::ofstream csv("phase4_scene_indices_summary.csv");
    writeCsvHeader(csv, columns);

    BruteForceCache bfCache;

    const std::vector<SceneSpec> scenes = buildSceneSpecs();
    const size_t totalCombos = scenes.size() * kKValues.size();
    size_t comboIndex = 0;

    // 每個 (particleNum, speedCV) 組合底下，broad+narrow 總時間最小的 K（近似 optimal K）。
    struct BestK {
        bool found = false;
        float k = 0.0f;
        double broadPlusNarrowS = std::numeric_limits<double>::infinity();
    };
    std::vector<BestK> bestPerScene(scenes.size());

    for (size_t sceneIdx = 0; sceneIdx < scenes.size(); ++sceneIdx) {
        const SceneSpec& spec = scenes[sceneIdx];
        std::vector<Particle> particles =
            scenario::synthesizeScene(spec.particleNum, kBoxSize, kRadius, kMeanSpeed, spec.speedCV, kAccMagnitude, spec.seed);

        SceneMetrics metrics = computeSceneMetrics(particles, kBoxSize);

        for (float k : kKValues) {
            ++comboIndex;

            auto t0 = std::chrono::high_resolution_clock::now();

            SimulationConfig cfg(
                kDt, k, /*hasSkin=*/true, Method::Octree,
                /*cellSize=*/1.0f, kOctreeMaxDepth, kOctreeLeafCapacity, kBoxSize
            );

            RunResult perf = runAndAverage(particles, cfg, kTotalFrames, kRepeatCount);
            // 計時區塊到這裡結束，正確性驗證獨立於計時之外進行（見 bench_runner.h）。
            CorrectnessCheck check =
                verifyAgainstBruteForce(particles, cfg, kTotalFrames, bfCache, spec.name, spec.seed);

            auto t1 = std::chrono::high_resolution_clock::now();
            double comboElapsedS = std::chrono::duration<double>(t1 - t0).count();

            if (!check.allMatch) {
                std::cerr << "[WARN] correctness mismatch: scene=" << spec.name << " K=" << k
                          << " firstMismatchFrame=" << check.firstMismatchFrame << "\n";
            }

            std::vector<std::string> row = {
                std::to_string(spec.particleNum),
                formatFixed(spec.speedCV, 2),
                formatFixed(metrics.solidFractionActual, 6),
                formatFixed(metrics.velocityCVActual, 6),
                formatFixed(metrics.accMagnitudeCVActual, 6),
                formatFixed(k, 0),
                formatFixed(perf.totalTimeS, 6),
                formatFixed(perf.broadPhaseTimeS, 6),
                formatFixed(perf.narrowPhaseTimeS, 6),
                std::to_string(perf.rebuildCount),
                formatFixed(perf.avgCandidatesPerFrame, 2),
                check.allMatch ? "1" : "0",
                std::to_string(kRepeatCount),
            };
            writeCsvRow(csv, columns, row);

            const double broadPlusNarrowS = perf.broadPhaseTimeS + perf.narrowPhaseTimeS;
            BestK& best = bestPerScene[sceneIdx];
            if (!best.found || broadPlusNarrowS < best.broadPlusNarrowS) {
                best.found = true;
                best.k = k;
                best.broadPlusNarrowS = broadPlusNarrowS;
            }

            std::cerr << "[" << comboIndex << "/" << totalCombos << "] "
                      << "particle_num=" << spec.particleNum << " speed_cv_param=" << spec.speedCV
                      << " solid_fraction_actual=" << formatFixed(metrics.solidFractionActual, 6)
                      << " velocity_cv_actual=" << formatFixed(metrics.velocityCVActual, 4)
                      << " K=" << formatFixed(k, 0)
                      << " total_time_s=" << formatFixed(perf.totalTimeS, 6)
                      << " correctness_ok=" << (check.allMatch ? 1 : 0)
                      << " elapsed=" << formatFixed(comboElapsedS, 2) << "s\n";
        }
    }

    csv.close();

    std::cerr << "\n=== phase4_scene_indices: approx optimal K by (particle_num, speed_cv) ===\n";
    for (size_t sceneIdx = 0; sceneIdx < scenes.size(); ++sceneIdx) {
        const SceneSpec& spec = scenes[sceneIdx];
        const BestK& best = bestPerScene[sceneIdx];
        std::cerr << "particle_num=" << spec.particleNum << " speed_cv_param=" << spec.speedCV
                   << " optimal_K=" << formatFixed(best.k, 0)
                   << " broad_plus_narrow_time_s=" << formatFixed(best.broadPlusNarrowS, 6) << "\n";
    }

    return 0;
}
