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
    double otherTimeMs = 0.0;
    bool didRebuild = false;

    PairList candidatePairs; // broad-phase
    PairList collisionPairs; // narrow-phase

    size_t candidateCountCache = 0;
    size_t collisionCountCache = 0;

    size_t candidateCount() const { return candidateCountCache; }
    size_t collisionCount() const { return collisionCountCache; }
};

class Simulation {
public:

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

    const FrameStats& step() {
        FrameStats stats;
        stats.frameIndex = currentFrame_;


        PairList collisions;

        if (config_.method == Method::BruteForce) {

            auto t0 = std::chrono::steady_clock::now();
            PairList brutePairs = bruteforce::BruteForce(particles_);
            auto t1 = std::chrono::steady_clock::now();

            stats.broadPhaseTimeMs = 0.0;
            stats.narrowPhaseTimeMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
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

        std::sort(collisions.begin(), collisions.end());

        stats.collisionCountCache = collisions.size();
        if (collectPairs_) stats.collisionPairs = collisions;

        auto t4 = std::chrono::steady_clock::now();
        resolveCollisions(collisions);
        integrate();
        auto t5 = std::chrono::steady_clock::now();
        stats.otherTimeMs = std::chrono::duration<double, std::milli>(t5 - t4).count();
        lastStats_ = std::move(stats);
        ++currentFrame_;
        return lastStats_;
    }


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


    void resolveCollisions(const PairList& collisions) {
        response::resolveCollisions(particles_, collisions);
    }

    std::vector<Particle> particles_;
    SimulationConfig config_;
    int currentFrame_ = 0;
    int totalFrames_ = 0;
    bool collectPairs_ = true;

    std::variant<broad::UniformGrid, broad::Octree> broadPhase_;
    PairList cachedCandidates_; 
    FrameStats lastStats_;
};