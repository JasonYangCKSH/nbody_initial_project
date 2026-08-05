
#pragma once
#include "nbody/force_calculator.h"
#include "nbody/octree.h"
#include <memory>

class BarnesHutCalculator : public ForceCalculator {
public:
    explicit BarnesHutCalculator(double G = 1.0, double softening = 0.0, double theta = 0.5)
        : G_(G), softening_(softening), theta_(theta) {}

    void computeAccelerations(std::vector<Body>& bodies) override;

    void setTheta(double theta) { theta_ = theta; }

private:
    double G_;
    double softening_;
    double theta_;

    // TODO: 建樹用的輔助函式
    std::unique_ptr<OctreeNode> buildTree(const std::vector<Body>& bodies);

    // TODO: 對單一粒子，遞迴walk整棵樹算加速度
    Vec3 computeForceOnBody(const Body& target, const OctreeNode& node);
};