#pragma once
#include "particle.h"
#include <vector>
#include <random>
#include <glm/glm.hpp>

// Simplified synthetic scenarios standing in for the paper's real test cases
// (hopper discharge, granular flow, avalanche, ...). Each one targets a flow
// regime the paper discusses rather than reproducing the exact geometry.
namespace scenario {

using Cloud = std::vector<Particle>;

// Roughly uniform, low-velocity cloud: mimics a settled / near-equilibrium
// bed, and the "homogeneous, MD-like" regime where a single global skin is
// expected to work reasonably well.
inline Cloud uniformCloud(int n, float boxSize, float radius, float speed, unsigned seed = 1) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> posDist(0.0f, boxSize);
    std::uniform_real_distribution<float> velDist(-speed, speed);

    Cloud particles(n);
    for (auto& p : particles) {
        p.pos = {posDist(rng), posDist(rng), posDist(rng)};
        p.vel = {velDist(rng), velDist(rng), velDist(rng)};
        p.radius = radius;
        p.posAtLastBroadPhase = p.pos;
    }
    return particles;
}

// Particles start nearly at rest and accelerate under gravity: velocity (and
// therefore the local skin) grows over the run, loosely mirroring the Hopper
// Discharge case (section 5.2).
inline Cloud freeFall(int n, float boxSize, float radius, unsigned seed = 2) {
    return uniformCloud(n, boxSize, radius, 0.02f, seed);
}

// Two populations with very different speeds sharing the same domain: the
// heterogeneous-flow-regime case the local (per-particle) skin is designed
// for, versus a single global skin (paper section 1: "different flow regimes
// coexist").
inline Cloud mixedRegime(int nSlow, int nFast, float boxSize, float radius,
                          float slowSpeed, float fastSpeed, unsigned seed = 3) {
    Cloud particles = uniformCloud(nSlow, boxSize, radius, slowSpeed, seed);
    Cloud fast = uniformCloud(nFast, boxSize, radius, fastSpeed, seed + 1);
    particles.insert(particles.end(), fast.begin(), fast.end());
    return particles;
}

// Particles radiating outward from the domain center at high, varied speed:
// stresses the automatic update condition (Eq. 5) with large, non-uniform
// per-step displacements.
inline Cloud explosion(int n, float boxSize, float radius, float speed, unsigned seed = 4) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dirDist(-1.0f, 1.0f);
    std::uniform_real_distribution<float> speedDist(0.2f * speed, speed);
    glm::vec3 center(boxSize * 0.5f);

    Cloud particles(n);
    for (auto& p : particles) {
        glm::vec3 dir = glm::normalize(glm::vec3(dirDist(rng), dirDist(rng), dirDist(rng)));
        p.pos = center + dir * 0.01f;
        p.vel = dir * speedDist(rng);
        p.radius = radius;
        p.posAtLastBroadPhase = p.pos;
    }
    return particles;
}





} // namespace scenario
