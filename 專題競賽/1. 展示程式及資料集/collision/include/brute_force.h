#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "narrow_phase.h"
namespace bruteforce{
inline PairList BruteForce(const std::vector<Particle>& particles) {
    PairList pairs;
    const size_t n = particles.size();
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = i + 1; j < n; ++j) {
            const Particle& a = particles[i];
            const Particle& b = particles[j];
            if ((a.radius + b.radius)*(a.radius + b.radius) >= glm::distance2(a.pos, b.pos)) {
                pairs.emplace_back(static_cast<int>(i), static_cast<int>(j));
            }
        }
    }
    return pairs;
}
}