#pragma once
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
    bool rebuild = false;
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
        applyCollisionResponse(particles_, stats.collisions);
        integrate(particles_);
        ++currentTimeFrame_;
        return stats;
    }

    StepStats stepSpatialStructure(StepStats& stats) {
        bool needsBuild = !hasList_ || !skinEnabled_ || !verlet::listStillValid(particles_);
        stats.rebuild = needsBuild;

        if (needsBuild) {
            auto broadStart = std::chrono::high_resolution_clock::now();
            cachedPairs_ = std::visit([this](auto& s) {
                return s.Build(particles_, skinEnabled_);
            }, *structure_);
            if (skinEnabled_) {
                verlet::updateLocalSkin(particles_, cfg_.K, cfg_.dt);
                verlet::capSkinToCellSize(particles_, cfg_.cellSize);
                verlet::recordBroadPhaseSnapshot(particles_);
            }
            auto broadEnd = std::chrono::high_resolution_clock::now();
            stats.broadPhaseSeconds = std::chrono::duration<double>(broadEnd - broadStart).count();
            hasList_ = true;
            stats.broadPhaseExecuted = true;
        }
        stats.broadPhasePairs = (int)cachedPairs_.size();

        auto narrowStart = std::chrono::high_resolution_clock::now();
        for (auto [i, j] : cachedPairs_) {
            if (narrow::colliding(particles_[i], particles_[j])) {
                ++stats.collisionCount;
                stats.collisions.emplace_back(i, j);
            }
        }
        auto narrowEnd = std::chrono::high_resolution_clock::now();
        stats.narrowPhaseSeconds = std::chrono::duration<double>(narrowEnd - narrowStart).count();

        applyCollisionResponse(particles_, stats.collisions);
        integrate(particles_);
        ++currentTimeFrame_;
        return stats;
    }

    // Penalty-based contact response: overlapping particles push each other
    // apart with a spring force (F = K * penetration), feeding into acc so
    // integrate() carries it into this step's velocity/position update.
    void applyCollisionResponse(std::vector<Particle>& particles,
                                 const std::vector<std::pair<int, int>>& collisions) {
        for (auto [i, j] : collisions) {
            Particle& a = particles[i];
            Particle& b = particles[j];
            glm::vec3 delta = a.pos - b.pos;
            float dist = glm::length(delta);
            float penetration = (a.radius + b.radius) - dist;
            if (penetration <= 0.0f) continue;
            glm::vec3 normal = (dist > 1e-6f) ? (delta / dist) : glm::vec3(0.0f, 1.0f, 0.0f);
            glm::vec3 force = cfg_.K * penetration * normal;
            a.acc += force;
            b.acc -= force;
        }
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
public:
    Simulation(int totalTimeFrame, StructureMode mode,
               std::unique_ptr<std::variant<broad::UniformGrid, broad::Octree>> structure,
               bool skinEnabled, SimConfig cfg = SimConfig{})
        : totalTimeFrame_(totalTimeFrame), mode_(mode),
          structure_(std::move(structure)), skinEnabled_(skinEnabled), cfg_(cfg) {}

    void InitializeParticles(std::vector<Particle> initial) {
        particles_ = std::move(initial);
        currentTimeFrame_ = 0;
    }

    std::vector<StepStats> runForFrames(int totalFrames) {
        std::vector<StepStats> history;
        history.reserve(totalFrames);

        const int remainingFrames = std::max(0, totalTimeFrame_ - currentTimeFrame_);
        const int framesToRun = std::min(totalFrames, remainingFrames);

        for (int i = 0; i < framesToRun; ++i) {
            history.push_back(step());
        }

        return history;
    }

};