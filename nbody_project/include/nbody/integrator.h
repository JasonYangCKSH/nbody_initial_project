#pragma once
#include "body.h"
#include "force_calculator.h"
#include <vector>

class LeapfrogIntegrator {
public:
    // TODO: 建構子，接受一個 ForceCalculator&（為什麼用reference？）

    // TODO: step函式，接受 bodies 跟 dt
    // 內部要做的三件事（Kick-Drift-Kick）：
    // 1. 每個body: velocity += acceleration * (dt * 0.5)
    // 2. 每個body: position += velocity * dt
    // 3. 呼叫 calculator 重新算 acceleration（因為位置變了）
    // 4. 每個body: velocity += acceleration * (dt * 0.5)   ← 第二次kick
    void step(std::vector<Body>& bodies, double dt) {
        // TODO
    }

private:
    // TODO: 需要儲存對ForceCalculator的參照
};