#include "Particle.h"



// 更新粒子位置與速度
// p.velocity += acceleration * dt
// p.position += p.velocity * dt
void Particle::update(double dt, const Eigen::Vector3d& acceleration) {
    velocity += acceleration * dt;  // 更新速度
    position += velocity * dt;      // 更新位置
}

// 設定初始狀態（常用於初始化場景）
void Particle::setState(const Eigen::Vector3d& pos, const Eigen::Vector3d& vel) {
    position = pos;
    velocity = vel;
}
