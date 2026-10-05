#pragma once
#include "particle.h"
#include <glm/glm.hpp>

// Narrow-phase
namespace narrow {

inline float overlap(const Particle& a, const Particle& b) {
    return (a.radius + b.radius)*(a.radius + b.radius) - glm::distance2(a.pos, b.pos);
}

inline bool colliding(const Particle& a, const Particle& b) {
    return overlap(a, b) >= 0.0f;
}

} // namespace narrow
