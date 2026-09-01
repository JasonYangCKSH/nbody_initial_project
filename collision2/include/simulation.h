#pragma once

#include <algorithm>
#include <chrono>
#include <variant>
#include <vector>

#include "particle.h"
#include "broad_phase.h"
#include "narrow_phase.h"
#include "verlet_buffer.h"
#include "brute_force.h"  // 若你 header 檔名不同，請對應修正
#include "collision_response.h"

// 要測試的 broad-phase / baseline 演算法
enum class Method {
    BruteForce,    // 單一獨立基準方法，不參與 broad-phase / narrow-phase 設定
    UniformGrid,
    Octree,
};

// 一次模擬跑期間固定不變的參數
struct SimulationConfig {
    float dt;
    float K;
    bool hasSkin;

    Method method;
    float worldSize;

    float cellSize;
    int maxDepth;
    int leafCapacity;
    SimulationConfig(
        float dt = 1.0f / 60.0f, float K = 2.0f, bool hasSkin = false,
        Method method = Method::UniformGrid, float cellSize = 1.0f,
        int maxDepth = 8, int leafCapacity = 8, float worldSize = 100.0f
    ): dt(dt),K(K),hasSkin(hasSkin),method(method),cellSize(cellSize),
       maxDepth(maxDepth),leafCapacity(leafCapacity),worldSize(worldSize){}
};

// 每一幀量測到的結果，供 bench/test 讀取
struct FrameStats {
    int frameIndex = 0;

    double broadPhaseTimeMs = 0.0;
    double narrowPhaseTimeMs = 0.0;
    bool didRebuild = false;

    // 只有 Simulation 建構時 collectPairs=true 才會填入完整內容；collectPairs=false
    // 時維持空的 PairList（省掉逐幀複製整份候選/碰撞清單的成本），數量請一律透過
    // candidateCount()/collisionCount() 讀取，不要直接看這兩個 vector 的 size()。
    PairList candidatePairs;  // 本幀有效的 broad-phase 候選清單
    PairList collisionPairs;  // narrow-phase 篩選後的實際碰撞清單

    size_t candidateCountCache = 0;
    size_t collisionCountCache = 0;

    size_t candidateCount() const { return candidateCountCache; }
    size_t collisionCount() const { return collisionCountCache; }
};

class Simulation {
public:
    // collectPairs 控制 step() 要不要把候選/碰撞的完整 PairList 複製進 FrameStats
    // （見 FrameStats 上方註解）；純效能量測（例如 bench_runner::runAndAverage()
    // 內部真正計時用的那幾次 repeat）應該關閉，避免複製整份 pair list 干擾計時。
    // 預設 true 是為了讓既有呼叫端（不傳這個參數的）行為維持不變。
    Simulation(std::vector<Particle> particles, SimulationConfig config, int totalFrames, bool collectPairs = true)
        : particles_(std::move(particles)),
          config_(config),
          totalFrames_(totalFrames),
          collectPairs_(collectPairs),
          broadPhase_(broad::UniformGrid(config.cellSize)) {
        if (config_.method == Method::Octree) {
            broadPhase_ = broad::Octree(config_.maxDepth, config_.leafCapacity, config_.worldSize);
        }
    }

    // 跑一幀：先做 collision 判定，再處理 collision response，最後才 integration
    const FrameStats& step() {
        FrameStats stats;
        stats.frameIndex = currentFrame_;

        // 本幀真正的碰撞清單：不管 collectPairs_ 開關與否都要完整算出來，因為
        // resolveCollisions() 一定要用這份資料驅動物理；collectPairs_ 只決定
        // 要不要「另外」複製一份存進 stats 給外部查閱（見 FrameStats 上方註解）。
        PairList collisions;

        if (config_.method == Method::BruteForce) {
            // 直接用 brute force 基準法，不走 broad-phase / narrow-phase 流程
            auto t0 = std::chrono::steady_clock::now();
            PairList brutePairs = BruteForce(particles_);
            auto t1 = std::chrono::steady_clock::now();

            stats.broadPhaseTimeMs = 0.0;
            stats.narrowPhaseTimeMs =
                std::chrono::duration<double, std::milli>(t1 - t0).count();

            stats.didRebuild = false;
            stats.candidateCountCache = brutePairs.size();
            if (collectPairs_) stats.candidatePairs = brutePairs;

            collisions = std::move(brutePairs);
        } else {
            stats.didRebuild = needsRebuild();
            if (stats.didRebuild) {
                auto t0 = std::chrono::steady_clock::now();
                cachedCandidates_ = buildBroadPhase();
                auto t1 = std::chrono::steady_clock::now();
                stats.broadPhaseTimeMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

                verlet::recordBroadPhaseSnapshot(particles_);
                if (config_.hasSkin) {
                    verlet::updateLocalSkin(particles_, config_.K, config_.dt);
                    if (config_.method == Method::UniformGrid) {
                        verlet::capSkinToCellSize(particles_, config_.cellSize);
                    } else if (config_.method == Method::Octree) {
                        if (const auto* octree = std::get_if<broad::Octree>(&broadPhase_)) {
                            verlet::capSkinToLeafExtent(particles_, octree->LeafHalfExtents(particles_));
                        }
                    }
                }
            }

            auto t2 = std::chrono::steady_clock::now();
            for (const auto& [i, j] : cachedCandidates_) {
                if (narrow::colliding(particles_[i], particles_[j])) {
                    collisions.emplace_back(i, j);
                }
            }
            auto t3 = std::chrono::steady_clock::now();
            stats.narrowPhaseTimeMs = std::chrono::duration<double, std::milli>(t3 - t2).count();

            stats.candidateCountCache = cachedCandidates_.size();
            if (collectPairs_) stats.candidatePairs = cachedCandidates_;
        }

        // resolveCollisions() 是逐對依序處理的衝量解法，同一幀內若有粒子同時涉及
        // 兩個以上碰撞（collision cluster），處理順序會影響結果。candidatePairs
        // 的順序取決於各 broad-phase 容器的走訪順序（unordered_map / leaf 走訪／…），
        // 跨方法本來就不同，所以先排成跟 BruteForce() 一致的 (i,j) 升冪順序，
        // 確保只要 broad-phase 沒漏抓碰撞，不同方法看到的處理順序也一致，
        // 軌跡才不會單純因為容器走訪順序不同而分岔。
        std::sort(collisions.begin(), collisions.end());

        stats.collisionCountCache = collisions.size();
        if (collectPairs_) stats.collisionPairs = collisions;

        // 先根據 collisionPairs 進行速度 / 加速度更新
        resolveCollisions(collisions);

        // 最後才做 integration（verlet 位置/速度/加速度更新）
        integrate();

        lastStats_ = std::move(stats);
        ++currentFrame_;
        return lastStats_;
    }

    // 跑滿 totalFrames，回傳每一幀的統計
    std::vector<FrameStats> run() {
        std::vector<FrameStats> history;
        history.reserve(totalFrames_);
        for (int i = 0; i < totalFrames_; ++i) {
            history.push_back(step());
        }
        return history;
    }

    const std::vector<Particle>& particles() const { return particles_; }
    const SimulationConfig& config() const { return config_; }
    int currentFrame() const { return currentFrame_; }
    int totalFrames() const { return totalFrames_; }
    const FrameStats& lastStats() const { return lastStats_; }

private:
    void integrate() {
        for (auto& p : particles_) {
            p.vel += p.acc * config_.dt;
            p.pos += p.vel * config_.dt;
        }
        response::reflectOffWalls(particles_, config_.worldSize);
    }

    bool needsRebuild() const {
        return currentFrame_ == 0 || !verlet::listStillValid(particles_);
    }

    PairList buildBroadPhase() const {
        return std::visit([this](const auto& bp) { return bp.Build(particles_, config_.hasSkin); }, broadPhase_);
    }

    // brute-force 基準法：完全不參與 broad-phase / narrow-phase 設定
    PairList computeBruteForcePairs() const {
        PairList pairs;
        pairs.reserve(particles_.size() * (particles_.size() - 1) / 2);

        for (size_t i = 0; i < particles_.size(); ++i) {
            for (size_t j = i + 1; j < particles_.size(); ++j) {
                if (narrow::colliding(particles_[i], particles_[j])) {
                    pairs.emplace_back(static_cast<int>(i), static_cast<int>(j));
                }
            }
        }

        return pairs;
    }

    // 依據 collisionPairs 進行碰撞回應：一般質量彈性碰撞（見 collision_response.h）
    void resolveCollisions(const PairList& collisions) {
        response::resolveCollisions(particles_, collisions);
    }

    std::vector<Particle> particles_;
    SimulationConfig config_;
    int currentFrame_ = 0;
    int totalFrames_ = 0;
    bool collectPairs_ = true;

    std::variant<broad::UniformGrid, broad::Octree> broadPhase_;
    PairList cachedCandidates_;  // 上次 rebuild 後沿用的候選清單
    FrameStats lastStats_;
};