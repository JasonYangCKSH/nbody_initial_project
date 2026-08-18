#pragma once
#include "particle.h"
#include <vector>
#include <algorithm>
#include <stdexcept>
// Local Verlet buffer approach (Checkaraou et al., "Local Verlet buffer
// approach for broad-phase interaction detection in Discrete Element
// Method", arXiv:2208.13770).
namespace verlet {

// Eq. 4 / Eq. 11: skin_p = K * v_p * dt, computed per particle from its own
// velocity rather than a single global bulk velocity.
inline void updateLocalSkin(std::vector<Particle>& particles, float K, float dt) {
    for (auto& p : particles) {
        p.skin = K * glm::length(p.vel) * dt;
    }
}

// Baseline comparison used in the paper (section 5.4 / [20]): a uniform skin
// equal to the particle radius, identical for every particle.
inline void updateSkinRadius(std::vector<Particle>& particles) {

    for (auto& p : particles) {
        p.skin = p.radius;
    }
}

// Eq. 12: the linked-cell method requires R_NL <= cell size, so the skin is
// capped such that R_C + skin never exceeds cellSize.
inline void capSkinToCellSize(std::vector<Particle>& particles, float cellSize) {
    assert(cellSize > 0.0f && "message");
    //if (cellSize <= 0.0f) throw std::invalid_argument("cellSize must be positive");

    for (auto& p : particles) {
        float maxSkin = std::max(cellSize * 0.5f - p.radius, 0.0f);
        p.skin = std::clamp(p.skin, 0.0f, maxSkin);
    }
}

// Condition (5): the Verlet list is still valid as long as no particle has
// moved further than its own skin distance since the last broad-phase.
inline bool listStillValid(const std::vector<Particle>& particles) {
    for (const auto& p : particles) {
        if (glm::length(p.pos - p.posAtLastBroadPhase) > p.skin) {
            return false;
        }
    }
    return true;
}

inline void recordBroadPhaseSnapshot(std::vector<Particle>& particles) {
    for (auto& p : particles) {
        p.posAtLastBroadPhase = p.pos;
    }
}

} // namespace verlet
