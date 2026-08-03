#pragma once
#include "nbody/body.h"
#include <array>
#include <memory>

class OctreeNode {
public:
    Vec3 center;
    double halfSize;

    Vec3 centerOfMass;
    double totalMass = 0.0;

    bool isLeaf = true;
    const Body* body = nullptr;

    std::array<std::unique_ptr<OctreeNode>, 8> children;

    OctreeNode(const Vec3& _center, double _halfSize)
        : center(_center), halfSize(_halfSize) {}


    // TODO: insert(const Body& b)
    // 把一個粒子插入這棵樹（遞迴邏輯）
    void insert(const Body& b) {
        // case 1: empty leaf node
        if (this->totalMass == 0.0) {
            this->body = &b;
            this->totalMass = b.mass;
            this->centerOfMass = b.position;
            return;
        }
        // case 2: nonempty leaf node
        if (this->isLeaf) {
            this->isLeaf = false;
            const Body *oldBody = this->body;
            this->body = nullptr;
            int oldIdx = getOctantIndex(oldBody->position);
            
            if (!children[oldIdx]) {
                children[oldIdx] = std::make_unique<OctreeNode>(childCenter(oldIdx), halfSize * 0.5);
            }
            children[oldIdx]->insert(*oldBody);
        }
        // case 3: internal node
        int idx = getOctantIndex(b.position);
        if (!children[idx]) {
            children[idx] = std::make_unique<OctreeNode>(childCenter(idx), halfSize * 0.5);
        }
        children[idx]->insert(b);
    }
    // TODO: 一個輔助函式，判斷某個位置屬於8個子節點中的哪一個
    // 通常回傳 0~7 的index
    int getOctantIndex(const Vec3& position) const {
        int index = 0;
        if (position.x >= center.x) index += 1;
        if (position.y >= center.y) index += 2;
        if (position.z >= center.z) index += 4;
        

        return index;
    }
    // TODO: computeMassDistribution()
    // Bottom-up：遞迴計算每個節點的totalMass跟centerOfMass
    void computeMassDistribution() {
        if (this->isLeaf) {
            // leaf node：質量分布就是它自己存的那個粒子（如果有的話）
            // totalMass / centerOfMass 已經在insert()時設好了
            return;
        }

        totalMass = 0.0;
        centerOfMass = Vec3(0.0, 0.0, 0.0);

        for (auto& child : children) {
            if (child) {
                child->computeMassDistribution();
                totalMass += child->totalMass;
                centerOfMass += child->centerOfMass * child->totalMass;
            }
        }

        if (totalMass > 0.0) {
            centerOfMass = centerOfMass * (1.0 / totalMass);
        }

    }
    Vec3 childCenter(int idx) const {
        double quarter = halfSize * 0.5;
        double dx = (idx & 1) ? quarter : -quarter;
        double dy = (idx & 2) ? quarter : -quarter;
        double dz = (idx & 4) ? quarter : -quarter;
        return Vec3(center.x + dx, center.y + dy, center.z + dz);
    }
};