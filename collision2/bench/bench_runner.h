#pragma once

// K-sweep benchmark：在固定的結構參數（cellSizeRatio / leafCapacity / maxDepth）
// 下，掃描 Verlet skin 的 K 值（等比取樣，1~1000），比較 uniform_grid(+skin) 與
// octree(+skin) 的 broad/narrow/response 三段時間、rebuild 次數、平均每次 rebuild
// 撈出的 candidate 數，並逐幀跟 brute force ground truth 比對 collisionPairs 以
// 確認正確性。輸出單一份 summary CSV（一個 combo 一列）。
//
// total_time 定義為「該次跑法裡所有幀的 broadPhaseTime+narrowPhaseTime+
// responsePhaseTime 加總」，不是包住整個 run() 的 wall-clock——這樣量到的是純演算法
// 成本，不含 FrameInfo 記錄 collisionPairs 這種跟效能無關的資料搬運開銷。

#include "simulation.h"
#include "scenario.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace benchrunner {

struct BenchmarkConfig {
    // scenario / 物理參數
    int particleCount = 1000;
    float worldSize = 60.0f;
    float particleRadius = 1.0f;
    float speed = 1.5f;
    float acc = 0.0f;
    float dt = 1.0f / 60.0f;
    int totalFrames = 1000;

    unsigned scenarioSeed = 100;
    float clusterFactor = 0.0f;
    float hotspotSpreadRatio = 0.03f * worldSize;  // 實際 hotspotSpread = hotspotSpreadRatio * worldSize
    int hotspotCount = 1;

    // 固定的結構參數（不掃描）
    float cellSizeRatio = 2.0f;
    int leafCapacity = 8;
    int maxDepth = 8;

    // 每個 combo 重複跑幾次以取平均/標準差
    int repeatCount = 10;

    // K 掃描點：等比取樣（1-2-5 級數），橫跨 1~1000 三個數量級
    std::vector<float> kValues = {1, 2, 5, 10, 20, 50, 100, 200, 500, 1000};

    float gridCellSize() const { return cellSizeRatio * 2.0f * particleRadius; }
};

class BenchmarkCSVWriter {
public:
    explicit BenchmarkCSVWriter(const std::string& path) : out_(path) {
        if (!out_) {
            throw std::runtime_error("BenchmarkCSVWriter: failed to open " + path);
        }
    }

    void writeHeader(const std::vector<std::string>& columns) { writeRow(columns); }

    void writeRow(const std::vector<std::string>& fields) {
        for (size_t i = 0; i < fields.size(); ++i) {
            out_ << fields[i];
            if (i + 1 < fields.size()) out_ << ",";
        }
        out_ << "\n";
    }

private:
    std::ofstream out_;
};

namespace detail {

// baseline 方法（brute_force / uniform_grid / octree，皆 hasSkin=false）不受 K 影響，
// 用這個 sentinel 跟真正被掃描到的 K 值區分開，CSV 裡印成 "NA"。
constexpr float kNotApplicableK = -1.0f;

inline std::string formatK(float k) {
    if (k == kNotApplicableK) return "NA";
    std::ostringstream oss;
    oss << static_cast<long long>(k);
    return oss.str();
}

inline std::string formatFixed(double value, int precision) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << value;
    return oss.str();
}

inline double mean(const std::vector<double>& v) {
    double s = 0.0;
    for (double x : v) s += x;
    return s / static_cast<double>(v.size());
}

// 樣本標準差（分母 n-1），衡量 repeatCount 次計時彼此的跳動幅度。
inline double stddev(const std::vector<double>& v, double m) {
    if (v.size() < 2) return 0.0;
    double s = 0.0;
    for (double x : v) s += (x - m) * (x - m);
    return std::sqrt(s / static_cast<double>(v.size() - 1));
}

// 單次 run 裡，把 1000 幀的三段時間各自加總（毫秒）。
struct RunTiming {
    double broadMs = 0.0;
    double narrowMs = 0.0;
    double responseMs = 0.0;
    double totalMs() const { return broadMs + narrowMs + responseMs; }
};

inline RunTiming sumTiming(const std::vector<FrameInfo>& frames) {
    RunTiming t;
    for (const auto& f : frames) {
        t.broadMs += f.broadPhaseTime;
        t.narrowMs += f.narrowPhaseTime;
        t.responseMs += f.responsePhaseTime;
    }
    return t;
}

// 只在有 rebuild 的那一幀累加 candidate 數，除以 rebuild 次數——
// 量的是「平均每次重建撈出多少候選對」，不會被沒 rebuild 的幀稀釋。
// （論文沒有明確定義這個指標，這是這份 benchmark 自訂的定義。）
inline double avgCandidatePerRebuild(const std::vector<FrameInfo>& frames, int rebuildCount) {
    if (rebuildCount <= 0) return 0.0;
    size_t total = 0;
    for (const auto& f : frames) {
        if (f.didRebuild) total += f.candidateCount();
    }
    return static_cast<double>(total) / static_cast<double>(rebuildCount);
}

// 逐幀比對 collisionPairs 是否跟 brute force ground truth 完全一致（兩邊在
// Simulation::step() 裡都已經排序過，可以直接用 vector 的 == 比較）。
// 回傳第一個不一致的 frame index，全部一致回傳 -1。
inline int firstMismatchFrame(const std::vector<FrameInfo>& frames,
                               const std::vector<PairList>& groundTruth) {
    const size_t n = std::min(frames.size(), groundTruth.size());
    for (size_t i = 0; i < n; ++i) {
        if (frames[i].collisionPairs != groundTruth[i]) return static_cast<int>(i);
    }
    return -1;
}

}  // namespace detail

class BenchmarkRunner {
public:
    BenchmarkRunner(const BenchmarkConfig& config, BenchmarkCSVWriter& writer)
        : config_(config), writer_(writer) {}

    void run() {
        writer_.writeHeader({
            "scenario", "structure_mode", "K",
            "total_time_avg_s", "total_time_std_s",
            "broad_time_avg_s", "narrow_time_avg_s", "response_time_avg_s",
            "rebuild_count", "avg_candidate_per_rebuild",
            "correctness_ok", "first_mismatch_frame", "repeat_count",
        });

        const std::vector<Particle> particles = buildScenario();
        const float gridCellSize = config_.gridCellSize();

        std::cerr << "[bench] brute_force (ground truth)...\n";
        const std::vector<PairList> groundTruth = runBruteForceReference(particles);

        std::cerr << "[bench] uniform_grid (baseline)...\n";
        runCombo("uniform_grid", detail::kNotApplicableK,
                 SimulationConfig(config_.particleRadius, config_.dt, /*K=*/0.0f, /*hasSkin=*/false,
                                   Method::UniformGrid, gridCellSize, config_.maxDepth,
                                   config_.leafCapacity, config_.worldSize),
                 particles, groundTruth);

        std::cerr << "[bench] octree (baseline)...\n";
        runCombo("octree", detail::kNotApplicableK,
                 SimulationConfig(config_.particleRadius, config_.dt, /*K=*/0.0f, /*hasSkin=*/false,
                                   Method::Octree, /*cellSize=*/1.0f, config_.maxDepth,
                                   config_.leafCapacity, config_.worldSize),
                 particles, groundTruth);

        for (float k : config_.kValues) {
            std::cerr << "[bench] uniform_grid_skin K=" << detail::formatK(k) << "...\n";
            runCombo("uniform_grid_skin", k,
                     SimulationConfig(config_.particleRadius, config_.dt, k, /*hasSkin=*/true,
                                       Method::UniformGrid, gridCellSize, config_.maxDepth,
                                       config_.leafCapacity, config_.worldSize),
                     particles, groundTruth);

            std::cerr << "[bench] octree_skin K=" << detail::formatK(k) << "...\n";
            runCombo("octree_skin", k,
                     SimulationConfig(config_.particleRadius, config_.dt, k, /*hasSkin=*/true,
                                       Method::Octree, /*cellSize=*/1.0f, config_.maxDepth,
                                       config_.leafCapacity, config_.worldSize),
                     particles, groundTruth);
        }

        std::cerr << "[bench] done.\n";
    }

private:
    const BenchmarkConfig& config_;
    BenchmarkCSVWriter& writer_;

    std::vector<Particle> buildScenario() const {
        return scenario::spatialCluster(
            config_.particleCount, config_.worldSize, config_.particleRadius, config_.speed,
            config_.acc, config_.clusterFactor, config_.hotspotSpreadRatio * config_.worldSize,
            config_.hotspotCount, config_.scenarioSeed);
    }

    // brute force 是全域唯一的 ground truth，且是純 O(n^2)、沒有 rebuild 概念，
    // 只跑一次：同一次 run() 同時拿來當 CSV 的 baseline 列，也拿來當逐幀比對基準，
    // 不用為了兩個目的各自重跑一次。
    std::vector<PairList> runBruteForceReference(const std::vector<Particle>& particles) {
        SimulationConfig cfg(config_.particleRadius, config_.dt, /*K=*/0.0f, /*hasSkin=*/false,
                              Method::BruteForce, /*cellSize=*/1.0f, config_.maxDepth,
                              config_.leafCapacity, config_.worldSize);
        Simulation sim(cfg);
        sim.initialize(particles, config_.totalFrames);
        sim.run();

        const auto& frames = sim.frameHistory();

        std::vector<PairList> ground;
        ground.reserve(frames.size());
        for (const auto& f : frames) ground.push_back(f.collisionPairs);

        const detail::RunTiming timing = detail::sumTiming(frames);
        writer_.writeRow({
            "spatial_cluster",
            "brute_force",
            "NA",
            detail::formatFixed(timing.totalMs() / 1000.0, 6),
            "NA",  // 只跑一次，沒有 std
            detail::formatFixed(timing.broadMs / 1000.0, 6),
            detail::formatFixed(timing.narrowMs / 1000.0, 6),
            detail::formatFixed(timing.responseMs / 1000.0, 6),
            "NA",  // rebuild 對 brute force 沒有意義
            "NA",  // avg_candidate_per_rebuild 同上
            "NA",  // 它自己就是 ground truth，沒有「跟誰比對」的問題
            "NA",
            "1",
        });

        return ground;
    }

    void runCombo(const std::string& structureMode, float kForCsv, const SimulationConfig& cfg,
                  const std::vector<Particle>& particles, const std::vector<PairList>& groundTruth) {
        Simulation sim(cfg);

        std::vector<double> totalMs, broadMs, narrowMs, responseMs;
        totalMs.reserve(config_.repeatCount);
        broadMs.reserve(config_.repeatCount);
        narrowMs.reserve(config_.repeatCount);
        responseMs.reserve(config_.repeatCount);

        int rebuildCount = 0;
        double avgCandidate = 0.0;
        int mismatchFrame = -1;

        for (int r = 0; r < config_.repeatCount; ++r) {
            // Simulation::initialize() 的參數是傳值，這裡傳同一份 master particles
            // 進去，每次 repeat 都會拿到一份新拷貝，不會被前一次 repeat 的模擬結果污染。
            sim.initialize(particles, config_.totalFrames);
            sim.run();

            const detail::RunTiming t = detail::sumTiming(sim.frameHistory());
            broadMs.push_back(t.broadMs);
            narrowMs.push_back(t.narrowMs);
            responseMs.push_back(t.responseMs);
            totalMs.push_back(t.totalMs());

            // 物理是決定性的，repeatCount 次的 rebuild 節奏跟碰撞結果理論上完全一樣，
            // 只需要在第一次 repeat 算 rebuild_count / avg_candidate / 正確性比對。
            if (r == 0) {
                rebuildCount = sim.rebuildCount();
                avgCandidate = detail::avgCandidatePerRebuild(sim.frameHistory(), rebuildCount);
                mismatchFrame = detail::firstMismatchFrame(sim.frameHistory(), groundTruth);
            }
        }

        const double totalMean = detail::mean(totalMs);
        const double totalStd = detail::stddev(totalMs, totalMean);
        const double broadMean = detail::mean(broadMs);
        const double narrowMean = detail::mean(narrowMs);
        const double responseMean = detail::mean(responseMs);

        writer_.writeRow({
            "spatial_cluster",
            structureMode,
            detail::formatK(kForCsv),
            detail::formatFixed(totalMean / 1000.0, 6),
            detail::formatFixed(totalStd / 1000.0, 6),
            detail::formatFixed(broadMean / 1000.0, 6),
            detail::formatFixed(narrowMean / 1000.0, 6),
            detail::formatFixed(responseMean / 1000.0, 6),
            std::to_string(rebuildCount),
            detail::formatFixed(avgCandidate, 2),
            mismatchFrame == -1 ? "1" : "0",
            mismatchFrame == -1 ? "NA" : std::to_string(mismatchFrame),
            std::to_string(config_.repeatCount),
        });

        std::cerr << "  -> total_time_avg_s=" << detail::formatFixed(totalMean / 1000.0, 6)
                  << " rebuild_count=" << rebuildCount
                  << " correctness=" << (mismatchFrame == -1 ? "OK" : ("MISMATCH@" + std::to_string(mismatchFrame)))
                  << "\n";
    }
};

}  // namespace benchrunner
