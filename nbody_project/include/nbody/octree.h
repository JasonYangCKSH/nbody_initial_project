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

};