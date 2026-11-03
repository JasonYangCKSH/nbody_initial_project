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
                                            float clusterFactor,
                                            float hotspotSpread,
                                            int hotspotCount,
                                            float fastRatio = 0.0f,
                                            float fastMult = 10.0,      
                                            unsigned seed = 124) {
assert(hotspotCount > 0 && "hotspot number cannot be negative.");
    std::mt19937 rng(seed);

    const float hotspotRange = boxSize * 0.4f;
    std::uniform_real_distribution<float> hotspotCenterDist(-hotspotRange, hotspotRange);

    std::vector<glm::vec3> hotspots(hotspotCount);
    for (auto& h: hotspots) {
        h = {hotspotCenterDist(rng), hotspotCenterDist(rng), hotspotCenterDist(rng)};
    }

    std::uniform_real_distribution<float> posDist(-boxSize * 0.5f, boxSize * 0.5f);
    std::uniform_real_distribution<float> velDist(-speed, speed);
    std::uniform_real_distribution<float> accDist(-acc, acc);
    std::uniform_real_distribution<float> clusterChoiceDist(0.0f, 1.0f);
    std::uniform_int_distribution<int> hotspotPickDist(0, hotspotCount - 1);
    std::normal_distribution<float> hotspotOffsetDist(0.0f, hotspotSpread);
    
    // 1. 最低成本新增：機率判斷分布
    std::uniform_real_distribution<float> fastChoiceDist(0.0f, 1.0f);

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

        // 2. 最低成本修改：抽中 fastRatio 的粒子直接套用倍率，且把原本硬編碼為 0 的 p.acc 補上
        float mult = (fastChoiceDist(rng) < fastRatio) ? fastMult : 1.0f;

        p.vel = glm::vec3(velDist(rng), velDist(rng), velDist(rng)) * mult;
        p.acc = glm::vec3(accDist(rng), accDist(rng), accDist(rng)) * mult; 
        
        p.radius = radius;
        p.posAtLastBroadPhase = p.pos;
    }

    return particles;
}

}
