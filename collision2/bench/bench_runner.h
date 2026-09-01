// bench_runner.h — 五支 phase*.cpp（phase1_structural_octree / phase1_structural_grid /
// phase2_k_sweep / phase3_dt_sweep / phase4_scene_indices）共用的 benchmark runner。
//
// 只負責「跑一次模擬、量測時間、跟 brute force ground truth 比對正確性、寫 CSV」這幾件
// 共通邏輯，不寫 main()，也不決定要掃哪些參數 —— 掃描變因（K/dt/scenario/structure_mode/
// scene 索引…）由各自的 phase*.cpp 決定。
//
// 使用順序上的重要提醒：
//   1) 先呼叫 runOnce() 或 runAndAverage() 拿到 RunResult（純效能計時）。
//   2) 計時區塊結束、RunResult 到手之後，「另外獨立」呼叫 verifyAgainstBruteForce()
//      做正確性驗證。驗證呼叫本身會再跑一次模擬，絕對不能被包進效能計時範圍，
//      否則量出來的時間會被驗證用的額外模擬污染，見 verifyAgainstBruteForce() 前的註解。

#pragma once

#include "simulation.h"

#include <algorithm>
#include <cmath>
#include <ostream>
#include <iomanip>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace bench_runner {

// ---------------------------------------------------------------------------
// 1. RunResult
// ---------------------------------------------------------------------------

struct RunResult {
    double totalTimeS = 0.0;
    double broadPhaseTimeS = 0.0;
    double narrowPhaseTimeS = 0.0;
    double otherTimeS = 0.0;
    int rebuildCount = 0;
    double avgCandidatesPerFrame = 0.0;
    size_t lastFrameCollisionCount = 0;

    // 只有 runAndAverage() 才會填這個欄位；runOnce() 單次呼叫沒有多筆樣本可以算
    // 標準差，維持預設值 0.0。
    double totalTimeStdS = 0.0;
};

// ---------------------------------------------------------------------------
// 2. runOnce
// ---------------------------------------------------------------------------

// 跑一次完整模擬（totalFrames 幀），只量測 Simulation::run() 本身的時間。
//
// particles 一律先複製一份再交給 Simulation —— Simulation 建構子可能會修改傳入的
// 粒子內容，呼叫方手上的 particles 必須維持乾淨的初始狀態，才能讓 runAndAverage()
// 或呼叫端自己的多次呼叫，每次都是從同一組全新初始條件開始，不會被前一次呼叫污染。
//
// collectPairs 直接轉傳給 Simulation 建構子（見 simulation.h 對 FrameStats /
// collectPairs 的說明）：純計時用的呼叫應該關閉，省掉逐幀複製整份候選/碰撞
// PairList 的成本，避免這份複製動作污染量到的時間；candidateCount()/
// collisionCount() 這兩個彙總數字不管開關與否都照樣正確（走的是 FrameStats
// 內部的 cache 欄位，不是複製出來的 PairList 本身），下面的加總邏輯不需要
// 因為這個開關而改變。
//
// 注意 Dead Code Elimination 風險：broadPhaseTimeMs / narrowPhaseTimeMs /
// candidateCount() / collisionCount() 這幾個量測值，這裡全部都會被累加寫進回傳的
// RunResult（並非「算完就丟」的中間值），呼叫端也一定會把 RunResult 寫進 CSV，
// 確保整條路徑上沒有「算完但沒被使用」的結果，避免 -O2/-O3 把量測邏輯優化掉。
inline RunResult runOnce(
    const std::vector<Particle>& particles, const SimulationConfig& cfg, int totalFrames, bool collectPairs = true
) {
    std::vector<Particle> particlesCopy(particles);
    Simulation sim(std::move(particlesCopy), cfg, totalFrames, collectPairs);

    // 計時只包住 run() 本身，不把後面的統計彙總算進去。
    auto t0 = std::chrono::steady_clock::now();
    std::vector<FrameStats> history = sim.run();
    auto t1 = std::chrono::steady_clock::now();

    RunResult result;
    result.totalTimeS = std::chrono::duration<double>(t1 - t0).count();

    double broadMs = 0.0;
    double narrowMs = 0.0;
    double candidateSum = 0.0;
    int rebuilds = 0;
    for (const auto& f : history) {
        broadMs += f.broadPhaseTimeMs;
        narrowMs += f.narrowPhaseTimeMs;
        candidateSum += static_cast<double>(f.candidateCount());
        if (f.didRebuild) ++rebuilds;
    }

    result.broadPhaseTimeS = broadMs / 1000.0;
    result.narrowPhaseTimeS = narrowMs / 1000.0;
    result.otherTimeS = result.totalTimeS - result.broadPhaseTimeS - result.narrowPhaseTimeS;
    result.rebuildCount = rebuilds;
    result.avgCandidatesPerFrame = history.empty() ? 0.0 : candidateSum / static_cast<double>(history.size());
    result.lastFrameCollisionCount = history.empty() ? size_t{0} : history.back().collisionCount();

    return result;
}

// ---------------------------------------------------------------------------
// 3. runAndAverage
// ---------------------------------------------------------------------------

// 呼叫 runOnce() repeatCount 次取平均。
//
// rebuildCount / avgCandidatesPerFrame / lastFrameCollisionCount 這幾個欄位不是計時
// 結果，而是同一組初始 particles + 同一個 cfg 底下完全 deterministic 的物理量
// （這個模擬是 detection-only，不做隨機性），每次 repeat 理論上都會得到一模一樣的值，
// 所以直接取最後一次 repeat 的值即可，不需要（也不應該）對它們取平均再引入多餘的
// 浮點誤差；只有 totalTimeS / broadPhaseTimeS / narrowPhaseTimeS / otherTimeS 這種
// wall-clock 計時結果才需要真的做平均與標準差。
inline RunResult runAndAverage(
    const std::vector<Particle>& particles, const SimulationConfig& cfg, int totalFrames, int repeatCount
) {
    std::vector<RunResult> runs;
    runs.reserve(static_cast<size_t>(repeatCount));
    for (int i = 0; i < repeatCount; ++i) {
        // 純計時用的 repeat，一律關閉 collectPairs：這裡只需要 RunResult 裡的彙總
        // 數字（時間、rebuildCount、avgCandidatesPerFrame…），不需要逐幀的完整
        // PairList，關閉可以避免複製整份候選/碰撞清單干擾計時。
        runs.push_back(runOnce(particles, cfg, totalFrames, /*collectPairs=*/false));
    }

    const double n = static_cast<double>(runs.size());
    double totalSum = 0.0, broadSum = 0.0, narrowSum = 0.0, otherSum = 0.0;
    for (const auto& r : runs) {
        totalSum += r.totalTimeS;
        broadSum += r.broadPhaseTimeS;
        narrowSum += r.narrowPhaseTimeS;
        otherSum += r.otherTimeS;
    }

    RunResult avg;
    avg.totalTimeS = totalSum / n;
    avg.broadPhaseTimeS = broadSum / n;
    avg.narrowPhaseTimeS = narrowSum / n;
    avg.otherTimeS = otherSum / n;

    const RunResult& last = runs.back();
    avg.rebuildCount = last.rebuildCount;
    avg.avgCandidatesPerFrame = last.avgCandidatesPerFrame;
    avg.lastFrameCollisionCount = last.lastFrameCollisionCount;

    double variance = 0.0;
    for (const auto& r : runs) {
        const double diff = r.totalTimeS - avg.totalTimeS;
        variance += diff * diff;
    }
    variance /= n;
    avg.totalTimeStdS = std::sqrt(variance);

    return avg;
}

// ---------------------------------------------------------------------------
// 4. BruteForceCache
// ---------------------------------------------------------------------------

// Ground truth 只跟粒子的物理軌跡有關，跟用哪個 broad-phase 方法測完全無關 ——
// 這個模擬是 detection-only，不做碰撞回應以外的分岔（resolveCollisions 只要
// candidatePairs 有把真正碰撞的 pair 涵蓋進去，處理順序就已經用 (i,j) 排序對齊，
// 見 simulation.h 裡 Simulation::step() 的排序註解），所以同一組初始條件不管
// method/K/structure_mode 是什麼，只要 (scenario, seed, dt, totalFrames) 相同，
// brute force 跑出來的碰撞歷史就完全一樣，可以安全地快取、重複利用。
class BruteForceCache {
public:
    using Key = std::tuple<std::string /*scenarioName*/, unsigned /*seed*/, float /*dt*/, int /*totalFrames*/>;

    // 每一幀一份「排序過的碰撞 pair id 列表」。FrameStats::collisionPairs 本身已經是
    // public 成員（simulation.h 裡的 PairList collisionPairs），所以這裡走的是完整
    // pair-level 版本，不是 count-only fallback：把每個 (i, j) pair 編碼成單一
    // size_t（i 存高 32 bit、j 存低 32 bit）方便存放跟比較，而不是只存
    // collisionCountPerFrame 這種退化版本。日後如果要比對 pair 的實際內容而非只是
    // packed 編碼，可以直接改用 collisionPairs 本身，不需要再改資料結構的外層形狀。
    using FrameCollisions = std::vector<size_t>;
    using RunCollisions = std::vector<FrameCollisions>;

    static size_t packPair(int i, int j) {
        return (static_cast<size_t>(static_cast<unsigned>(i)) << 32) | static_cast<size_t>(static_cast<unsigned>(j));
    }

    // worldSize 沒有放進 cache key —— 跟 method/K/structure_mode 一樣，同一個
    // scenarioName 底下 worldSize 理應是固定不變的場景參數，不是掃描變因。但
    // Simulation::integrate() 每一幀都會用 worldSize 做 reflectOffWalls()，這會
    // 真的影響粒子軌跡，所以「跑」ground truth 的時候仍然必須拿到正確的 worldSize
    // （否則邊界不一樣，ground truth 跟 cfgUnderTest 從某一幀開始就會分岔）。
    // 這裡用預設參數延續 SimulationConfig 的預設值，呼叫方應該一律傳入
    // cfgUnderTest.worldSize，讓 ground truth 用跟受測設定完全相同的邊界。
    const RunCollisions& getOrCompute(
        const std::vector<Particle>& particles, const std::string& scenarioName, unsigned seed, float dt,
        int totalFrames, float worldSize = SimulationConfig().worldSize
    ) {
        Key key(scenarioName, seed, dt, totalFrames);
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            return it->second;
        }

        std::vector<Particle> particlesCopy(particles);
        SimulationConfig groundTruthCfg(
            dt, /*K=*/0.0f, /*hasSkin=*/false, Method::BruteForce, /*cellSize=*/1.0f, /*maxDepth=*/8,
            /*leafCapacity=*/8, worldSize
        );
        // collectPairs 一律開啟：下面要逐幀讀 f.collisionPairs 的完整內容來編碼、
        // 快取成 ground truth，只有 count 是不夠的（見 simulation.h 對 collectPairs
        // 的說明）。
        Simulation sim(std::move(particlesCopy), groundTruthCfg, totalFrames, /*collectPairs=*/true);
        std::vector<FrameStats> history = sim.run();

        RunCollisions collisions;
        collisions.reserve(history.size());
        for (const auto& f : history) {
            FrameCollisions packed;
            packed.reserve(f.collisionPairs.size());
            for (const auto& pair : f.collisionPairs) {
                packed.push_back(packPair(pair.first, pair.second));
            }
            // collisionPairs 在 Simulation::step() 裡已經排序過（見 simulation.h），
            // packPair() 對同一組 (i, j) 保序，這裡不需要再排一次。
            collisions.push_back(std::move(packed));
        }

        auto insertedPair = cache_.emplace(std::move(key), std::move(collisions));
        return insertedPair.first->second;
    }

private:
    struct KeyHash {
        size_t operator()(const Key& key) const noexcept {
            size_t h = std::hash<std::string>{}(std::get<0>(key));
            auto combine = [&h](size_t v) { h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2); };
            combine(std::hash<unsigned>{}(std::get<1>(key)));
            combine(std::hash<float>{}(std::get<2>(key)));
            combine(std::hash<int>{}(std::get<3>(key)));
            return h;
        }
    };

    std::unordered_map<Key, RunCollisions, KeyHash> cache_;
};

// ---------------------------------------------------------------------------
// 5. verifyAgainstBruteForce
// ---------------------------------------------------------------------------

struct CorrectnessCheck {
    bool allMatch = true;
    int firstMismatchFrame = -1;  // 沒有不一致的幀就維持 -1
};

// 用 cache 取得（或計算一次並快取）brute force ground truth，針對 cfgUnderTest
// 重新跑一次模擬，逐幀比對排序過的 collisionPairs 是否完全一致。
//
// **重要：呼叫方務必只在效能計時區塊「結束之後」才呼叫這個函式。** 這裡會自己再跑一次
// cfgUnderTest 的完整模擬來取得受測結果，如果把這個呼叫包進 runOnce()/runAndAverage()
// 前後的計時範圍內，量到的時間就會多算一次驗證用的模擬，數字會失真。正確用法固定是：
//   RunResult perf = runOnce(particles, cfg, totalFrames);   // 或 runAndAverage(...)
//   // ↑ 計時區塊到這裡結束
//   CorrectnessCheck check =                                  // 獨立呼叫，不計時
//       verifyAgainstBruteForce(particles, cfg, totalFrames, cache, scenarioName, seed);
inline CorrectnessCheck verifyAgainstBruteForce(
    const std::vector<Particle>& particles, const SimulationConfig& cfgUnderTest, int totalFrames,
    BruteForceCache& cache, const std::string& scenarioName, unsigned seed
) {
    const BruteForceCache::RunCollisions& groundTruth =
        cache.getOrCompute(particles, scenarioName, seed, cfgUnderTest.dt, totalFrames, cfgUnderTest.worldSize);

    std::vector<Particle> particlesCopy(particles);
    // collectPairs 一律開啟：下面逐幀比對要用 history[frame].collisionPairs 的完整
    // 內容，只有 count 沒辦法逐 pair 比對（見 simulation.h 對 collectPairs 的說明）。
    Simulation sim(std::move(particlesCopy), cfgUnderTest, totalFrames, /*collectPairs=*/true);
    std::vector<FrameStats> history = sim.run();

    CorrectnessCheck result;
    const size_t frameCount = std::min(history.size(), groundTruth.size());
    for (size_t frame = 0; frame < frameCount; ++frame) {
        BruteForceCache::FrameCollisions actual;
        actual.reserve(history[frame].collisionPairs.size());
        for (const auto& pair : history[frame].collisionPairs) {
            actual.push_back(BruteForceCache::packPair(pair.first, pair.second));
        }
        // history[frame].collisionPairs 一樣已經在 Simulation::step() 裡排序過，
        // 跟 groundTruth[frame] 的排序方式一致，可以直接逐一比對。

        if (actual != groundTruth[frame]) {
            result.allMatch = false;
            result.firstMismatchFrame = static_cast<int>(frame);
            return result;
        }
    }

    return result;
}

// ---------------------------------------------------------------------------
// 6. CSV writer helpers
// ---------------------------------------------------------------------------

// 一欄的名稱、欄寬（<=0 表示不對齊，照原始長度輸出）跟對齊方向。欄寬不寫死成常數，
// 由呼叫端（各 phase*.cpp）自行決定要用多寬，傳進來即可。
struct CsvColumn {
    std::string name;
    int width = 0;
    bool leftAlign = true;
};

inline void writeCsvHeader(std::ostream& os, const std::vector<CsvColumn>& columns) {
    for (size_t i = 0; i < columns.size(); ++i) {
        if (i > 0) os << ',';
        const CsvColumn& c = columns[i];
        if (c.width > 0) {
            os << (c.leftAlign ? std::left : std::right) << std::setw(c.width) << c.name;
        } else {
            os << c.name;
        }
    }
    os << '\n';
}

// values 由呼叫端先格式化成字串（例如浮點數要印幾位小數，交給呼叫端用
// std::ostringstream + std::fixed/std::setprecision 決定），這裡只負責逗號分隔
// 跟依照 columns 裡的欄寬做對齊。
inline void writeCsvRow(std::ostream& os, const std::vector<CsvColumn>& columns, const std::vector<std::string>& values) {
    const size_t n = std::min(columns.size(), values.size());
    for (size_t i = 0; i < n; ++i) {
        if (i > 0) os << ',';
        const CsvColumn& c = columns[i];
        if (c.width > 0) {
            os << (c.leftAlign ? std::left : std::right) << std::setw(c.width) << values[i];
        } else {
            os << values[i];
        }
    }
    os << '\n';
}

}  // namespace bench_runner
