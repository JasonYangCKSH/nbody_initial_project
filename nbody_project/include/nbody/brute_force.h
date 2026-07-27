#pragma once
#include "force_calculator.h"

class BruteForceCalculator : public ForceCalculator {
public:
    // TODO: 建構子，接受 G（重力常數）跟 softening（softening length）
    // 兩個都給預設值：G預設1.0，softening預設0.0

    // TODO: override 上面介面定義的函式
    void computeAccelerations(std::vector<Body>& bodies) override;

private:
    // TODO: 需要儲存 G 跟 softening 這兩個成員變數
};