#pragma once
#include "particle.h"
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <iostream>

namespace verlet {

inline void updateLocalSkin(std::vector<Particle>& particles, float K, float dt) {
    for (auto& p : particles) {
        p.skin = K * glm::length(p.vel) * dt + 0.5f * K * K * glm::length(p.acc) * dt * dt;
    }
}

inline void updateSkinRadius(std::vector<Particle>& particles) {

    for (auto& p : particles) {
        p.skin = p.radius;
    }
}

inline void capSkinToCellSize(std::vector<Particle>& particles, float cellSize) {
    assert(cellSize > 0.0f && "message");
    //if (cellSize <= 0.0f) throw std::invalid_argument("cellSize must be positive");

    for (auto& p : particles) {
        float maxSkin = std::max(cellSize / 2 - p.radius, 0.0f);
        p.skin = std::clamp(p.skin, 0.0f, maxSkin);
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