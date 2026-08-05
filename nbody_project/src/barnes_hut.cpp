#include "nbody/barnes_hut.h"
#include <algorithm>
#include <limits>
#include <cmath>

std::unique_ptr<OctreeNode> BarnesHutCalculator::buildTree(const std::vector<Body>& bodies) {
    double minX = std::numeric_limits<double>::max(), maxX = -std::numeric_limits<double>::max();
    double minY = std::numeric_limits<double>::max(), maxY = -std::numeric_limits<double>::max();
    double minZ = std::numeric_limits<double>::max(), maxZ = -std::numeric_limits<double>::max();

    for (const auto& b : bodies) {
        minX = std::min(minX, b.position.x); 
        maxX = std::max(maxX, b.position.x);
        minY = std::min(minY, b.position.y); 
        maxY = std::max(maxY, b.position.y);
        minZ = std::min(minZ, b.position.z); 
        maxZ = std::max(maxZ, b.position.z);
    }

    Vec3 center((minX + maxX) * 0.5, (minY + maxY) * 0.5, (minZ + maxZ) * 0.5);
    double spanX = maxX - minX, spanY = maxY - minY, spanZ = maxZ - minZ;
    double halfSize = std::max({spanX, spanY, spanZ}) * 0.5 + 1e-6; // 避免halfSize為0

    auto root = std::make_unique<OctreeNode>(center, halfSize);
    for (const auto& b : bodies) {
        root->insert(b);
    }
    root->computeMassDistribution();
    return root;
}

Vec3 BarnesHutCalculator::computeForceOnBody(const Body& target, const OctreeNode& node) {
    if (node.totalMass == 0.0) {
        return Vec3(0.0, 0.0, 0.0);
    }

    // 如果這個node是leaf，而且存的粒子剛好就是target自己，跳過（不跟自己算力）
    if (node.isLeaf && node.body == &target) {
        return Vec3(0.0, 0.0, 0.0);
    }

    Vec3 r = node.centerOfMass - target.position;
    double distSqr = r.dot(r) + softening_ * softening_;
    double dist = std::sqrt(distSqr);

    double s = node.halfSize * 2.0; // 這個node代表的空間大小

    // 判斷：是leaf，或是 s/d < theta，就直接用這個node近似
    if (node.isLeaf || (s / dist) < theta_) {
        double invDist3 = 1.0 / (distSqr * dist);
        return r * (G_ * node.totalMass * invDist3);
    }

    // 否則遞迴展開子節點
    Vec3 totalForce(0.0, 0.0, 0.0);
    for (const auto& child : node.children) {
        if (child) {
            totalForce += computeForceOnBody(target, *child);
        }
    }
    return totalForce;
}

void BarnesHutCalculator::computeAccelerations(std::vector<Body>& bodies) {
    if (bodies.empty()) return;

    auto root = buildTree(bodies);

    for (auto& b : bodies) {
        b.acceleration = computeForceOnBody(b, *root);  
    }
}