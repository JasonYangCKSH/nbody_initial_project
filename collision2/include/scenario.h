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

// Particles radiating outward from the domain center at high, varied speed:
// stresses the automatic update condition with large, non-uniform per-step
// displacements.
//
// center 對齊 broad::Octree 的 root（原點），見 uniformCloud() 上方的說明。
// 跟 uniformCloud() 的差別：uniformCloud 用 uniform_real_distribution(-speed, speed)
// 對每一軸獨立取樣，速度大小的變異程度完全被 speed 這個上界綁死，沒辦法在固定平均
// 速度下單獨調整「速度異質性」。這裡改成先用常態分佈取樣「速度大小」（mean=meanSpeed,
// std=meanSpeed*speedCV，取絕對值避免負值），方向另外用均勻隨機單位向量取樣，
// 讓 meanSpeed 跟 speedCV 兩者可以獨立控制。
//
// 位置分佈、acc 的取樣方式（每軸獨立 uniform(-accMagnitude, accMagnitude)）沿用
// uniformCloud() 的設計，座標系跟 broad::Octree 的 root 定義一致（見 uniformCloud()
// 上方的說明）。
inline std::vector<Particle> synthesizeScene(
    int particleNum, float boxSize, float radius, float meanSpeed, float speedCV, float accMagnitude,
    unsigned seed
) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> posDist(-boxSize * 0.5f, boxSize * 0.5f);
    std::normal_distribution<float> speedMagDist(meanSpeed, meanSpeed * speedCV);
    std::uniform_real_distribution<float> dirDist(-1.0f, 1.0f);
    std::uniform_real_distribution<float> accDist(-accMagnitude, accMagnitude);

    std::vector<Particle> particles(particleNum);
    for (auto& p : particles) {
        p.pos = {posDist(rng), posDist(rng), posDist(rng)};

        // 速度大小取絕對值避免常態分佈取樣出負值（speedCV 夠大時尾端機率不為零），
        // 方向再另外用均勻隨機單位向量取樣，兩者獨立，速度大小的變異不會混進方向裡。
        float speedMag = std::abs(speedMagDist(rng));
        glm::vec3 dir = glm::normalize(glm::vec3(dirDist(rng), dirDist(rng), dirDist(rng)));
        p.vel = dir * speedMag;

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

}
