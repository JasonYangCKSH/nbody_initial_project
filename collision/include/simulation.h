#pragma once
#include "particle.h"
#include "broad_phase.h"
#include "narrow_phase.h"
#include "verlet_buffer.h"
#include <vector>
#include <chrono>
#include <glm/glm.hpp>

// Per-step timing/diagnostics, mirroring the metrics the paper reports in
// Fig. 11 (broad/narrow-phase time) and Fig. 12 (skipped broad-phase %).
struct StepStats {
    bool broadPhaseExecuted = false;
    double broadPhaseSeconds = 0.0;
    double narrowPhaseSeconds = 0.0;
    int broadPhasePairs = 0;
    std::vector<std::pair<int, int>> collisions; // only filled when requested
    int collisionCount = 0;
};

struct SimConfig {
    float dt = 0.001f;
    float K = 200.0f;       // paper's recommended default (section 5.4)
    float cellSize = 1.0f;  // must be >= largest extended bounding-sphere diameter

    enum class SkinMode { LocalVelocity, FixedRadius, None };
    SkinMode skinMode = SkinMode::LocalVelocity;
};

// Drives one XDEM-style iteration (Fig. 5): decide whether the Verlet list
// needs rebuilding, run broad-phase if so, always run narrow-phase on the
// current list, then integrate.
class Simulation {
public:
    explicit Simulation(SimConfig cfg) : cfg_(cfg), cell_(cfg.cellSize) {}

    StepStats step(std::vector<Particle>& particles, bool recordCollisions = false) {
        StepStats stats;

        applySkinModel(particles);

        bool needsBuild = !hasList_ || !verlet::listStillValid(particles);
        if (needsBuild) {
            auto t0 = std::chrono::steady_clock::now();
            bool withSkin = cfg_.skinMode != SimConfig::SkinMode::None;
            verletList_ = cell_.build(particles, withSkin);
            auto t1 = std::chrono::steady_clock::now();

            verlet::recordBroadPhaseSnapshot(particles);
            hasList_ = true;
            stats.broadPhaseExecuted = true;
            stats.broadPhaseSeconds = std::chrono::duration<double>(t1 - t0).count();
        }
        stats.broadPhasePairs = (int)verletList_.size();

        auto t2 = std::chrono::steady_clock::now();
        int collisionCount = 0;
        if (recordCollisions) stats.collisions.reserve(verletList_.size());
        for (auto [i, j] : verletList_) {
            if (narrow::colliding(particles[i], particles[j])) {
                ++collisionCount;
                if (recordCollisions) stats.collisions.emplace_back(i, j);
            }
        }
        auto t3 = std::chrono::steady_clock::now();
        stats.narrowPhaseSeconds = std::chrono::duration<double>(t3 - t2).count();
        stats.collisionCount = collisionCount;

        integrate(particles);
        return stats;
    }

private:
    SimConfig cfg_;
    broad::LinkedCell cell_;
    PairList verletList_;
    bool hasList_ = false;

    void applySkinModel(std::vector<Particle>& particles) {
        switch (cfg_.skinMode) {
            case SimConfig::SkinMode::LocalVelocity:
                verlet::updateLocalSkin(particles, cfg_.K, cfg_.dt);
                verlet::capSkinToCellSize(particles, cfg_.cellSize);
                break;
            case SimConfig::SkinMode::FixedRadius:
                verlet::updateSkinRadius(particles);
                verlet::capSkinToCellSize(particles, cfg_.cellSize);
                break;
            case SimConfig::SkinMode::None:
                for (auto& p : particles) p.skin = 0.0f;
                break;
        }
    }

    // Gravity-only velocity-Verlet integrator (paper section 3.1: "leapfrog
    // and velocity-Verlet schemes"). Kept deliberately simple: this test
    // environment targets collision *detection*, not collision *response*.
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
