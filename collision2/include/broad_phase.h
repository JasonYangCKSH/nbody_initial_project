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
        bool isLeaf() const { return children[0] == nullptr; }
    };

    static int childIndex(const glm::vec3& center, const glm::vec3& pos) {
        int idx = 0;
        if (pos.x >= center.x) idx |= 1;
        if (pos.y >= center.y) idx |= 2;
        if (pos.z >= center.z) idx |= 4;
        return idx;
    }

    static glm::vec3 childCenter(const glm::vec3& parentCenter, float parentHalfExtent, int childIdx) {
        float q = parentHalfExtent * 0.5f;
        glm::vec3 offset(
            (childIdx & 1) ? q : -q,
            (childIdx & 2) ? q : -q,
            (childIdx & 4) ? q : -q);
        return parentCenter + offset;
    }

    void insert(Node* node, const std::vector<Particle>& particles, int idx, int depth) const {
        if (node->isLeaf()) {
            node->indices.push_back(idx);
            if (static_cast<int>(node->indices.size()) > leafCapacity_ && depth < maxDepth_) {
                split(node, particles, depth);
            }
            return;
        }
        int c = childIndex(node->center, particles[idx].pos);
        insert(node->children[c].get(), particles, idx, depth + 1);
    }

    void split(Node* node, const std::vector<Particle>& particles, int depth) const {
        for (int c = 0; c < 8; ++c) {
            node->children[c] = std::make_unique<Node>();
            node->children[c]->center = childCenter(node->center, node->halfExtent, c);
            node->children[c]->halfExtent = node->halfExtent * 0.5f;
        }

        std::vector<int> existing;
        existing.swap(node->indices);
        for (int idx : existing) {
            insert(node, particles, idx, depth);
        }
    }

    void collectLeaves(Node* node, std::vector<Node*>& leaves) const {
        if (node->isLeaf()) {
            if (!node->indices.empty()) leaves.push_back(node);
            return;
        }
        for (auto& child : node->children) collectLeaves(child.get(), leaves);
    }

    static void addPair(int a, int b, PairList& pairs) {
        if (a > b) std::swap(a, b);
        pairs.emplace_back(a, b);
    }

    static bool boxesOverlap(const Node* a, const Node* b, float margin) {
        glm::vec3 d = glm::abs(a->center - b->center);
        float reach = a->halfExtent + b->halfExtent + margin;
        return d.x <= reach && d.y <= reach && d.z <= reach;
    }


    void collectPairs(const std::vector<Node*>& leaves, const std::vector<Particle>& particles,
                       bool withSkin, PairList& pairs) const {
        float maxReach = 0.0f;
        for (const auto& p : particles) {
            maxReach = std::max(maxReach, p.radius + (withSkin ? p.skin : 0.0f));
        }
        float margin = 2.0f * maxReach;

        for (size_t i = 0; i < leaves.size(); ++i) {
            const auto& idxA = leaves[i]->indices;
            for (size_t a = 0; a < idxA.size(); ++a)
                for (size_t b = a + 1; b < idxA.size(); ++b) {
                    int ia = idxA[a], ib = idxA[b];
                    float skinA = (withSkin) ? particles[ia].skin : 0.0f;
                    float skinB = (withSkin) ? particles[ib].skin : 0.0f;
                    float r_nl_a = particles[ia].radius + skinA;
                    float r_nl_b = particles[ib].radius + skinB;
                    if ((r_nl_a + r_nl_b) * (r_nl_a + r_nl_b) > glm::distance2(particles[ia].pos, particles[ib].pos)) {
                        addPair(ia, ib, pairs);
                    }
                }

            for (size_t j = i + 1; j < leaves.size(); ++j) {
                if (!boxesOverlap(leaves[i], leaves[j], margin)) continue;
                for (int a : idxA)
                    for (int b : leaves[j]->indices) {
                        float skinA = (withSkin) ? particles[a].skin : 0.0f;
                        float skinB = (withSkin) ? particles[b].skin : 0.0f;
                        float r_nl_a = particles[a].radius + skinA;
                        float r_nl_b = particles[b].radius + skinB;
                        if ((r_nl_a + r_nl_b) * (r_nl_a + r_nl_b) > glm::distance2(particles[a].pos, particles[b].pos)) {
                            addPair(a, b, pairs);
                        }
                    }
            }
        }
    }

public:
    Octree(int maxDepth, int leafCapacity, float worldSize)
        : maxDepth_(maxDepth), leafCapacity_(leafCapacity), worldSize_(worldSize) {}

    PairList Build(const std::vector<Particle>& particles, bool withSkin) const {
        Node root;
        root.center = glm::vec3(0.0f);
        root.halfExtent = worldSize_ * 0.5f;

        for (int i = 0; i < static_cast<int>(particles.size()); ++i) {
            insert(&root, particles, i, 0);
        }

        std::vector<Node*> leaves;
        collectLeaves(&root, leaves);


    #ifndef NDEBUG
        for (const Node* leaf : leaves) {
            for (int idx : leaf->indices) {
                assert(leaf->halfExtent >= particles[idx].radius &&
                    "leaf halfExtent smaller than particle radius: "
                    "either leafCapacity is too small for this particle density, "
                    "or maxDepth allows splitting past a sane physical scale");
            }
        }
    #endif


        PairList pairs;
        collectPairs(leaves, particles, withSkin, pairs);
        return pairs;
    }


    std::vector<float> LeafHalfExtents(const std::vector<Particle>& particles) const {
        Node root;
        root.center = glm::vec3(0.0f);
        root.halfExtent = worldSize_ * 0.5f;

        for (int i = 0; i < static_cast<int>(particles.size()); ++i) {
            insert(&root, particles, i, 0);
        }

        std::vector<Node*> leaves;
        collectLeaves(&root, leaves);

        std::vector<float> halfExtents(particles.size(), 0.0f);
        for (Node* leaf : leaves) {
            for (int idx : leaf->indices) {
                halfExtents[idx] = leaf->halfExtent;
            }
        }
        return halfExtents;
    }
};

} // namespace broad