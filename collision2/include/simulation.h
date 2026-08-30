#pragma once

#include <chrono>
#include <variant>
#include <vector>

#include "particle.h"
#include "broad_phase.h"
#include "narrow_phase.h"
#include "verlet_buffer.h"

// 要測試的 broad-phase 演算法
enum class BroadPhaseMethod {
    UniformGrid,
    Octree,
};

// 一次模擬跑期間固定不變的參數
struct SimulationConfig {
    // 基礎
    float dt = 1.0f / 60.0f;
    float K = 2.0f;  // skin 公式的 K 值

    BroadPhaseMethod method = BroadPhaseMethod::UniformGrid;

    // uniform grid 專屬
    float cellSize = 1.0f;

    // octree 專屬
    int maxDepth = 8;
    int leafCapacity = 8;
    float worldSize = 100.0f;
};

// 每一幀量測到的結果，供 bench/test 讀取
struct FrameStats {
    int frameIndex = 0;

    double broadPhaseTimeMs = 0.0;
    double narrowPhaseTimeMs = 0.0;
    bool didRebuild = false;

    PairList candidatePairs;  // 本幀有效的 broad-phase 候選清單（rebuild 或沿用上次的）
    PairList collisionPairs;  // narrow-phase 篩選後的實際碰撞清單

    size_t candidateCount() const { return candidatePairs.size(); }
    size_t collisionCount() const { return collisionPairs.size(); }
};

class Simulation {
public:
    Simulation(std::vector<Particle> particles, SimulationConfig config, int totalFrames)
        : particles_(std::move(particles)),
          config_(config),
          totalFrames_(totalFrames),
          broadPhase_(broad::UniformGrid(config.cellSize)) {
        if (config_.method == BroadPhaseMethod::Octree) {
            broadPhase_ = broad::Octree(config_.maxDepth, config_.leafCapacity, config_.worldSize);
        }
    }

    // 跑一幀：積分位置 -> 視需要 rebuild broad-phase -> narrow-phase，回傳本幀統計
    const FrameStats& step() {
        integrate();

        FrameStats stats;
        stats.frameIndex = currentFrame_;

        stats.didRebuild = needsRebuild();
        if (stats.didRebuild) {
            auto t0 = std::chrono::high_resolution_clock::now();
            cachedCandidates_ = buildBroadPhase();
            auto t1 = std::chrono::high_resolution_clock::now();
            stats.broadPhaseTimeMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

            verlet::recordBroadPhaseSnapshot(particles_);
            verlet::updateLocalSkin(particles_, config_.K, config_.dt);
            if (config_.method == BroadPhaseMethod::UniformGrid) {
                verlet::capSkinToCellSize(particles_, config_.cellSize);
            }
        }

        auto t2 = std::chrono::high_resolution_clock::now();
        PairList collisions;
        for (const auto& [i, j] : cachedCandidates_) {
            if (narrow::colliding(particles_[i], particles_[j])) {
                collisions.emplace_back(i, j);
            }
        }
        auto t3 = std::chrono::high_resolution_clock::now();
        stats.narrowPhaseTimeMs = std::chrono::duration<double, std::milli>(t3 - t2).count();

        stats.candidatePairs = cachedCandidates_;
        stats.collisionPairs = std::move(collisions);

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
    }

    bool needsRebuild() const {
        return currentFrame_ == 0 || !verlet::listStillValid(particles_);
    }

    PairList buildBroadPhase() const {
        return std::visit([this](const auto& bp) { return bp.Build(particles_, true); }, broadPhase_);
    }

    std::vector<Particle> particles_;
    SimulationConfig config_;
    int currentFrame_ = 0;
    int totalFrames_ = 0;

    std::variant<broad::UniformGrid, broad::Octree> broadPhase_;
    PairList cachedCandidates_;  // 上次 rebuild 後沿用的候選清單
    FrameStats lastStats_;
};
