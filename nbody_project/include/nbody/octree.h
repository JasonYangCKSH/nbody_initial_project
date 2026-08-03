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

    // TODO: 一個輔助函式，判斷某個位置屬於8個子節點中的哪一個
    // 通常回傳 0~7 的index

    // TODO: computeMassDistribution()
    // Bottom-up：遞迴計算每個節點的totalMass跟centerOfMass
};