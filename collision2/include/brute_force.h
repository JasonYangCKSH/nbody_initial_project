#pragma once
#include <glm/glm.hpp>
#include <vector>
inline PairList BruteForce(const std::vector<Particle>& particles, bool withSkin) {
    PairList pairs;
    const size_t n = particles.size();
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = i + 1; j < n; ++j) {
            float radiusSum = particles[i].radius + particles[j].radius;
            if (withSkin) radiusSum += particles[i].skin + particles[j].skin;
            float dist = glm::distance2(particles[i].pos, particles[j].pos); 
            if (dist <= radiusSum * radiusSum) {
                pairs.emplace_back(static_cast<int>(i), static_cast<int>(j));
            }
        }
    }
    return pairs;
}