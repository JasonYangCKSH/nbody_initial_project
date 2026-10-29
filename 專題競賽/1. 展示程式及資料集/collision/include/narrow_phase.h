#pragma once
#include "particle.h"
#include <glm/glm.hpp>

// Narrow-phase: exact sphere-sphere collision test (paper's Eq. 2).
namespace narrow {

inline float overlap(const Particle& a, const Particle& b) {
    return a.radius + b.radius - glm::length(a.pos - b.pos);
}

inline bool colliding(const Particle& a, const Particle& b) {
    return overlap(a, b) > 0.0f;
}

} // namespace narrow
