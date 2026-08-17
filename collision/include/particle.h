# pragma once
#include <glm/glm.hpp>
class Particle {
public:
    // three basic
    glm::vec3 pos;
    glm::vec3 vel;
    glm::vec3 acc;
    float radius;
private:
    Pariticle(): pos(0.0f), vel(0.0f), acc(0.0f), radius(0.0f){}
    void update() {
        // Step 1: 用目前速度與加速度，推算下一刻位置
        pos += vel * dt + acc * (dt * dt * 0.5f);
        // Step 2: 根據新位置，重新計算加速度
        glm::vec3 newAcc = computeAcceleration();
        // Step 3: 用「舊加速度」與「新加速度」的平均值更新速度
        vel += (acc + newAcc) * (dt * 0.5f);
        // Step 4: 寫回新的加速度，供下一步使用
        acc = newAcc;
    }

};