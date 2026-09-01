#pragma once
#include "particle.h"
#include "broad_phase.h"
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <iostream>

namespace verlet {

inline void updateLocalSkin(std::vector<Particle>& particles, float K, float dt) {
    for (auto& p : particles) {
        p.skin = K * glm::length(p.vel) * dt + 0.5f * K * K  * glm::length(p.acc) * dt * dt;
    }
}



inline void capSkinToCellSize(std::vector<Particle>& particles, float cellSize) {
    assert(cellSize > 0.0f && "message");

    
    for (auto& p : particles) {
        const float maxSkin = cellSize / 2.0f - p.radius;
        p.skin = std::clamp(p.skin, 0.0f, maxSkin);
    }
}
inline void capSkinToLeafExtent(std::vector<Particle>& particles, const std::vector<float>& leafHalfExtents) {
    assert(particles.size() == leafHalfExtents.size() && "message");
    
    for (size_t idx = 0; idx < particles.size(); ++idx) {
        const float maxSkin = leafHalfExtents[idx] - particles[idx].radius;
        particles[idx].skin = std::clamp(particles[idx].skin, 0.0f, std::max(0.0f, maxSkin));
    }
}
inline bool listStillValid(const std::vector<Particle>& particles) {
    for (const auto& p : particles) {
        float disp = glm::length(p.pos - p.posAtLastBroadPhase);
        if (disp > p.skin) {
            
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
};