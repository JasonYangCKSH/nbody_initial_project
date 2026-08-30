#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include "particle.h"
#include "broad_phase.h"
#include "narrow_phase.h"
#include "verlet_buffer.h"
#include "brute_force.h"
#include "scenario.h"
#include <vector>
#include <chrono>
#include <iostream>
#include <memory>
#include <variant>
#include <glm/glm.hpp>

struct StepStats {
    bool broadPhaseExecuted = false;
    double broadPhaseSeconds = 0.0;
    double narrowPhaseSeconds = 0.0;
    int broadPhasePairs = 0;
    int collisionCount = 0;
    // TODO: 你覺得還需要記錄哪些？(對應你之前想的CSV欄位)
    bool rebuild;
    std::vector<std::pair<int, int>> collisions;
};
struct SimConfig {
    float dt = 0.001f;
    float K = 200.0f;
    float cellSize = 1.0f;

};
enum class StructureMode {
    BruteForce,
    UniformGrid,
    Octree
};

class Simulation {
private:
    int totalTimeFrame_;
    int currentTimeFrame_ = 0;
    std::vector<Particle> particles_;

    StructureMode mode_;
    SimConfig cfg_;
    std::unique_ptr<std::variant<broad::UniformGrid, broad::Octree>> structure_;  // 只有UniformGrid/Octree用得到,brute force時可以是nullptr
    bool skinEnabled_;

    PairList cachedPairs_;
    bool hasList_ = false;
public:
    Simulation(int totalTimeFrame, StructureMode mode,
               std::unique_ptr<std::variant<broad::UniformGrid, broad::Octree>> structure,
               bool skinEnabled)
        : totalTimeFrame_(totalTimeFrame), mode_(mode),
          structure_(std::move(structure)), skinEnabled_(skinEnabled) {}
    void InitializeParticles(std::vector<Particle> initial) {
        particles_ = std::move(initial);
    }
    StepStats step() {
        StepStats stats;
        if (mode_ == StructureMode::BruteForce)
            return stepBruteForce(stats);
        
        return stepSpatialStructure(stats);
    }

    StepStats stepBruteForce(StepStats& stats) {
        // TODO: 你自己想一下這裡該怎麼寫
        //       提示：brute::bruteForce() 直接回傳的是candidate pairs
        //       還是已經是真正碰撞的配對？回頭看你的 bruteForce() 實作，
        //       裡面用的判定式 dist <= rSum，這個算出來的東西，
        //       已經是「真正碰撞」還是還只是「候選配對」？
    }

    StepStats stepSpatialStructure(StepStats& stats) {
        // 這裡沿用共同介面的邏輯（Build → narrow-phase）
        bool needsBuild = !hasList_ || (skinEnabled_ && !verlet::listStillValid(particles_));
        // TODO: 上次提醒過你的 bug 還在這裡——
        //       skinEnabled_==false 時，這個判斷式對嗎？

        if (needsBuild) {
            cachedPairs_ = std::visit([this](auto& s) {
                return s.Build(particles_, skinEnabled_);
            }, *structure_);
            if (skinEnabled_) verlet::recordBroadPhaseSnapshot(particles_);
            hasList_ = true;
            stats.broadPhaseExecuted = true;
        }
        stats.broadPhasePairs = (int)cachedPairs_.size();

        int collisions = 0;
        for (auto [i, j] : cachedPairs_) {
            if (narrow::colliding(particles_[i], particles_[j])) ++collisions;
        }
        stats.collisionCount = collisions;

        integrate(particles_);
        ++currentTimeFrame_;
        return stats;
    }

    void integrate(std::vector<Particle>& particles) { /* TODO */ }
};