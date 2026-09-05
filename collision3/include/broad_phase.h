#pragma once
#include <vector>
#include <utility>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <unordered_map>
#include <memory>
#include <array>
#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>
#include "particle.h"



namespace broad {


class UniformGrid {
private:
    float cellSize_;

 
public:
    explicit UniformGrid(float cellSize) : cellSize_(cellSize) {}

    PairList Build(const std::vector<Particle>& particles, bool withSkin) const {

    }
};


class Octree {
private:
    int maxDepth_;
    int leafCapacity_;
    float worldSize_;

    struct Node {
        glm::vec3 center{0.0f};
        float halfExtent = 0.0f;
        std::vector<int> indices; 
        std::array<std::unique_ptr<Node>, 8> children; 
        bool isLeaf() const { return children[0] == nullptr; }
    };

 

public:
 
};

} // namespace broad