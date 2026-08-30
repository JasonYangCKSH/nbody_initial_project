#pragma once

#include "particle.h"
#include <vector>
#include <random>

namespace scenario {

inline std::vector<Particle> two_particle_bounce_scenario() {
    std::vector<Particle> particles(2);

    particles[0].pos = glm::vec3(-0.60f, 0.0f, 0.0f);
    particles[0].vel = glm::vec3(1.0f, 0.0f, 0.0f);
    particles[0].acc = glm::vec3(0.0f, 0.0f, 0.0f);
    particles[0].radius = 0.5f;
    particles[0].skin = 0.05f;

    particles[1].pos = glm::vec3(0.60f, 0.0f, 0.0f);
    particles[1].vel = glm::vec3(-1.0f, 0.0f, 0.0f);
    particles[1].acc = glm::vec3(0.0f, 0.0f, 0.0f);
    particles[1].radius = 0.5f;
    particles[1].skin = 0.05f;

    return particles;
}

// Roughly uniform, low-velocity cloud: mimics a settled / near-equilibrium
// bed, and the "homogeneous, MD-like" regime where a single global skin is
// expected to work reasonably well.
inline std::vector<Particle> uniformCloud(int n, float boxSize, float radius, float speed, unsigned seed = 1) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> posDist(0.0f, boxSize);
    std::uniform_real_distribution<float> velDist(-speed, speed);

    std::vector<Particle> particles(n);
    for (auto& p : particles) {
        p.pos = {posDist(rng), posDist(rng), posDist(rng)};
        p.vel = {velDist(rng), velDist(rng), velDist(rng)};
        p.radius = radius;
        p.posAtLastBroadPhase = p.pos;
    }
    return particles;
}

// Particles radiating outward from the domain center at high, varied speed:
// stresses the automatic update condition with large, non-uniform per-step
// displacements.
inline std::vector<Particle> explosion(int n, float boxSize, float radius, float speed, unsigned seed = 4) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dirDist(-1.0f, 1.0f);
    std::uniform_real_distribution<float> speedDist(0.2f * speed, speed);
    glm::vec3 center(boxSize * 0.5f);

    std::vector<Particle> particles(n);
    for (auto& p : particles) {
        glm::vec3 dir = glm::normalize(glm::vec3(dirDist(rng), dirDist(rng), dirDist(rng)));
        p.pos = center + dir * 0.01f;
        p.vel = dir * speedDist(rng);
        p.radius = radius;
        p.posAtLastBroadPhase = p.pos;
    }
    return particles;
}

}
