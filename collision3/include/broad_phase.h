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

    glm::ivec3 posToCell(const glm::vec3& pos) const {
        return glm::ivec3(glm::floor(pos / cellSize_));
    }

    static int64_t key(glm::ivec3 c) {
        constexpr int64_t OFFSET = 1 << 20;
        int64_t x = c.x + OFFSET, y = c.y + OFFSET, z = c.z + OFFSET;
        return (x << 42) ^ (y << 21) ^ z;
    }

    static glm::ivec3 decode(int64_t k) {
        constexpr int64_t OFFSET = 1 << 20;
        constexpr int64_t MASK = (1 << 21) - 1;
        int64_t z = k & MASK;
        int64_t y = (k >> 21) & MASK;
        int64_t x = (k >> 42) & MASK;
        return glm::ivec3(static_cast<int>(x - OFFSET), static_cast<int>(y - OFFSET), static_cast<int>(z - OFFSET));
    }

public:
    explicit UniformGrid(float cellSize) : cellSize_(cellSize) {}

    PairList Build(const std::vector<Particle>& particles, bool withSkin) const {
    #ifndef NDEBUG
        float maxRadius = 0.0f;
        for (const auto& p : particles) {
            maxRadius = std::max(maxRadius, p.radius);
        }
        assert(cellSize_ >= 2.0f * maxRadius &&
            "cellSize must be at least the largest particle's diameter, "
            "otherwise the neighbor-cell search (Algorithm 1) can miss collisions");
    #endif
        std::unordered_map<int64_t, std::vector<int>> cells;
    
        cells.reserve(particles.size());
        for (size_t i = 0; i < particles.size(); ++i) {
            cells[key(posToCell(particles[i].pos))].push_back(static_cast<int>(i));
        }
        static const glm::ivec3 kForwardOffsets[13] = {
            {1, 0, 0}, {1, 1, 0}, {0, 1, 0}, {-1, 1, 0},
            {1, 0, -1}, {1, 1, -1}, {0, 1, -1}, {-1, 1, -1},
            {1, 0, 1}, {1, 1, 1}, {0, 1, 1}, {-1, 1, 1},
            {0, 0, 1},
        };

        PairList pairs;
        for (const auto& [k, cellA] : cells) {
            glm::ivec3 base = decode(k);

            for (size_t a = 0; a < cellA.size(); ++a) {
                for (size_t b = a + 1; b < cellA.size(); ++b) {
                    int ia = cellA[a], ib = cellA[b];
                    float skinA = (withSkin)? particles[ia].skin: 0.0f;
                    float skinB = (withSkin)? particles[ib].skin: 0.0f;
                    float r_nl_a = particles[ia].radius + skinA;
                    float r_nl_b = particles[ib].radius + skinB;
                    if ((r_nl_a + r_nl_b) * (r_nl_a + r_nl_b) > glm::distance2(particles[ia].pos, particles[ib].pos)) {
                        int x = ia, y = ib;
                        if (x > y) std::swap(x, y);
                        pairs.emplace_back(x, y);
                    }
                }
            }

            for (const auto& offset : kForwardOffsets) {
                auto it = cells.find(key(base + offset));
                if (it == cells.end()) continue;
                for (int a : cellA) {
                    for (int b : it->second) {
                        float skinA = (withSkin) ? particles[a].skin: 0.0f;
                        float skinB = (withSkin) ? particles[b].skin: 0.0f;
                        float r_nl_a = particles[a].radius + skinA;
                        float r_nl_b = particles[b].radius + skinB;
                        if ((r_nl_a + r_nl_b) * (r_nl_a + r_nl_b) > glm::distance2(particles[a].pos, particles[b].pos)) {
                            int x = a, y = b;
                            if (x > y) std::swap(x, y);
                            pairs.emplace_back(x, y);
                        }
                    }
                }
            }
        }

        return pairs;
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
        bool isLeaf() const { return children[0] == nullptr;} 
    };

 

public:
    PairList Build(const std::vector<Particle>& particles, bool withSkin) const {


    }
};

} // namespace broad
