#pragma once
#include "nbody/body.h"
#include <array>
#include <memory>

class OctreeNode {
public:
    Vec3 center;
    double halfsize;

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
};