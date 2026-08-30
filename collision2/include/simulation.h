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
        for (int i = 0; i < (int)particles_.size(); ++i) {
            for (int j = i + 1; j < (int)particles_.size(); ++j) {
                if (narrow::colliding(particles_[i], particles_[j])) {
                    ++stats.collisionCount;
                    stats.collisions.emplace_back(i, j);
                }
            }
        }
        integrate(particles_);
        ++currentTimeFrame_;
        return stats;
    }

    StepStats stepSpatialStructure(StepStats& stats) {
        bool needsBuild = !hasList_ || !skinEnabled_ || !verlet::listStillValid(particles_);

        if (needsBuild) {
            cachedPairs_ = std::visit([this](auto& s) {
                return s.Build(particles_, skinEnabled_);
            }, *structure_);
            if (skinEnabled_) verlet::recordBroadPhaseSnapshot(particles_);
            hasList_ = true;
            stats.broadPhaseExecuted = true;
        }
        stats.broadPhasePairs = (int)cachedPairs_.size();

        for (auto [i, j] : cachedPairs_) {
            if (narrow::colliding(particles_[i], particles_[j])) {
                ++stats.collisionCount;
                stats.collisions.emplace_back(i, j);
            }
        }

        integrate(particles_);
        ++currentTimeFrame_;
        return stats;
    }

    void integrate(std::vector<Particle>& particles) {
        static const glm::vec3 gravity(0.0f, -9.81f, 0.0f);
        for (auto& p : particles) {
            p.pos += p.vel * cfg_.dt + 0.5f * p.acc * cfg_.dt * cfg_.dt;
            glm::vec3 newAcc = gravity;
            p.vel += 0.5f * (p.acc + newAcc) * cfg_.dt;
            p.acc = newAcc;
        }
    }
};