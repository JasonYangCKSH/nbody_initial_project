#pragma once

#include "particle.h"
#include <cmath>
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
//
// 座標以原點為中心，範圍 [-boxSize/2, boxSize/2]，跟 broad::Octree 的 root
// 定義一致（root.center = origin, halfExtent = worldSize/2，見 broad_phase.h），
// 這樣 boxSize 才能直接當 SimulationConfig::worldSize 使用。
inline std::vector<Particle> uniformCloud(int n, float boxSize, float radius, float speed, float acc,  unsigned seed = 1) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> posDist(-boxSize * 0.5f, boxSize * 0.5f);
    std::uniform_real_distribution<float> velDist(-speed, speed);
    std::uniform_real_distribution<float> accDist(-acc, acc);
    std::vector<Particle> particles(n);
    for (auto& p : particles) {
        p.pos = {posDist(rng), posDist(rng), posDist(rng)};
        p.vel = {velDist(rng), velDist(rng), velDist(rng)};
        p.acc = {accDist(rng), accDist(rng), accDist(rng)};
        p.radius = radius;
        p.posAtLastBroadPhase = p.pos;
    }
    return particles;
}



inline std::vector<Particle> explosion(int n, float boxSize, float radius, float speed, unsigned seed = 4) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dirDist(-1.0f, 1.0f);
    std::uniform_real_distribution<float> speedDist(0.2f * speed, speed);
    glm::vec3 center(0.0f);

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
inline std::vector<Particle> spatialCluster(int n, float boxSize, float radius, float  speed, float acc,
                                            float clusterFactor,  // 0: uniform  distributed 1: all  particles cluster in hotspot
                                            float hotspotSpread,
                                            int hotspotCount,      // 熱點數量
                                            unsigned seed = 5) {
    assert(hotspotCount > 0 && "hotspot number cannot be negative.");
    std::mt19937 rng(seed);


    const float hotspotRange = boxSize * 0.4f;
    std::uniform_real_distribution<float> hotspotCenterDist(-hotspotRange, hotspotRange);

    std::vector<glm::vec3> hotspots(hotspotCount);
    for (auto& h: hotspots) {
        h = {hotspotCenterDist(rng), hotspotCenterDist(rng),  hotspotCenterDist(rng)};
    }

    std::uniform_real_distribution<float> posDist(-boxSize * 0.5f, boxSize * 0.5f);
    std::uniform_real_distribution<float> velDist(-speed, speed);
    std::uniform_real_distribution<float> accDist(-acc, acc);
    std::uniform_real_distribution<float> clusterChoiceDist(0.0f, 1.0f);
    std::uniform_int_distribution<int> hotspotPickDist(0, hotspotCount - 1);
    std::normal_distribution<float> hotspotOffsetDist(0.0f, hotspotSpread);

    // 不管落在均勻分支還是熱點分支，一律 clamp 進「扣掉粒子半徑」的合法範圍，
    // 確保生成當下就沒有粒子違反 domain 邊界（跟粒子本身的物理大小一致）。
    const float clampMin = -boxSize * 0.5f + radius;
    const float clampMax = boxSize * 0.5f - radius;

    std::vector<Particle> particles(n);
    for (auto& p: particles) {
        glm::vec3 pos;
        if (clusterChoiceDist(rng) < clusterFactor) {
            const glm::vec3& center = hotspots[hotspotPickDist(rng)];
            pos = center + glm::vec3(hotspotOffsetDist(rng), hotspotOffsetDist(rng), hotspotOffsetDist(rng));
        } else {
            pos = {posDist(rng), posDist(rng), posDist(rng)};
        }

        pos.x = std::clamp(pos.x, clampMin, clampMax);
        pos.y = std::clamp(pos.y, clampMin, clampMax);
        pos.z = std::clamp(pos.z, clampMin, clampMax);
        p.pos = pos;

        p.vel = {velDist(rng), velDist(rng), velDist(rng)};
        p.acc = {accDist(rng), accDist(rng),  accDist(rng)};
        p.radius = radius;
        p.posAtLastBroadPhase = p.pos;
    }

    return particles;
}

}
