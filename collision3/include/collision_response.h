#pragma once
#include "particle.h"
#include <glm/glm.hpp>
#include <vector>

// 碰撞回應（一般質量彈性碰撞）與世界邊界反彈。Simulation 跟 test_correctness.cpp
// 的動態測試共用這一份實作，避免同樣的物理公式在兩處各自維護、彼此漂移。
namespace response {

// 針對每一對真實碰撞的粒子施加彈性碰撞衝量（沿碰撞法線，一般質量版本），
// 並做位置修正把重疊部分依質量反比推開，避免同一對粒子連續多幀卡在一起。
inline void resolveCollisions(std::vector<Particle>& particles, const PairList& collisions,
                               float restitution = 1.0f) {
    for (const auto& [i, j] : collisions) {
        auto& a = particles[i];
        auto& b = particles[j];

        glm::vec3 delta = b.pos - a.pos;
        float dist = glm::length(delta);
        if (dist < 1e-6f) continue;  // 位置重合，法線無意義，避免除以零

        glm::vec3 n = delta / dist;

        glm::vec3 relVel = a.vel - b.vel;
        float velAlongNormal = glm::dot(relVel, n);
        if (velAlongNormal <= 0.0f) continue;  // 已經在分離或平行，不需要施加衝量

        float invMassA = 1.0f / a.mass;
        float invMassB = 1.0f / b.mass;
        float impulseMag = -(1.0f + restitution) * velAlongNormal / (invMassA + invMassB);
        glm::vec3 impulse = impulseMag * n;

        a.vel += impulse * invMassA;
        b.vel -= impulse * invMassB;

        float overlap = (a.radius + b.radius) - dist;
        if (overlap > 0.0f) {
            glm::vec3 correction = n * (overlap / (invMassA + invMassB));
            a.pos -= correction * invMassA;
            b.pos += correction * invMassB;
        }
    }
}

// 世界邊界反彈：world 以原點為中心，範圍 [-worldSize/2, worldSize/2]，
// 跟 broad::Octree 的 root 定義一致（見 broad_phase.h）。
inline void reflectOffWalls(std::vector<Particle>& particles, float worldSize) {
    const float half = worldSize * 0.5f;
    for (auto& p : particles) {
        for (int axis = 0; axis < 3; ++axis) {
            const float lo = -half + p.radius;
            const float hi = half - p.radius;
            if (p.pos[axis] < lo) {
                p.pos[axis] = lo;
                p.vel[axis] = -p.vel[axis];
            } else if (p.pos[axis] > hi) {
                p.pos[axis] = hi;
                p.vel[axis] = -p.vel[axis];
            }
        }
    }
}

}  // namespace response
