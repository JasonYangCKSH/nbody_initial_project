#pragma once
#include "particle.h"
#include <glm/glm.hpp>
#include <vector>
#include <utility>
#include <unordered_map>
#include <cstdint>

using PairList = std::vector<std::pair<int, int>>;

namespace broad {

// O(n^2) reference broad-phase. Used both as the "no Verlet buffer" baseline
// (withSkin = false, rebuilt every step) and as ground truth to validate the
// linked-cell + Verlet buffer implementation below.
inline PairList bruteForce(const std::vector<Particle>& particles, bool withSkin) {
    PairList pairs;
    const size_t n = particles.size();
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = i + 1; j < n; ++j) {
            float rSum = particles[i].radius + particles[j].radius;
            if (withSkin) rSum += particles[i].skin + particles[j].skin;
            float dist = glm::length(particles[i].pos - particles[j].pos);
            if (dist <= rSum) pairs.emplace_back((int)i, (int)j);
        }
    }
    return pairs;
}

// Linked-cell broad-phase (paper's Algorithm 1), O(n). Requires cellSize to
// be at least as large as the largest extended bounding sphere diameter, and
// that skin has already been capped so R_NL = R_C + skin <= cellSize
// (Eq. 12, see verlet::capSkinToCellSize).
class LinkedCell {
public:
    explicit LinkedCell(float cellSize) : cellSize_(cellSize) {}

    PairList build(const std::vector<Particle>& particles, bool withSkin) const {
        std::unordered_map<int64_t, std::vector<int>> cells;
        for (size_t i = 0; i < particles.size(); ++i) {
            cells[key(cellOf(particles[i].pos))].push_back((int)i);
        }

        // The 13 "forward" neighbour offsets plus the cell itself visit every
        // unordered pair of cells exactly once (half-shell stencil), mirroring
        // the index(Cb) < index(Ca) de-duplication rule in Algorithm 1.
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
                    tryAdd(particles, cellA[a], cellA[b], withSkin, pairs);
                }
            }

            for (const auto& off : kForwardOffsets) {
                auto it = cells.find(key(base + off));
                if (it == cells.end()) continue;
                for (int a : cellA) {
                    for (int b : it->second) {
                        tryAdd(particles, a, b, withSkin, pairs);
                    }
                }
            }
        }
        return pairs;
    }

private:
    float cellSize_;

    glm::ivec3 cellOf(const glm::vec3& pos) const {
        return glm::ivec3(glm::floor(pos / cellSize_));
    }

    static void tryAdd(const std::vector<Particle>& particles, int a, int b,
                        bool withSkin, PairList& pairs) {
        float rSum = particles[a].radius + particles[b].radius;
        if (withSkin) rSum += particles[a].skin + particles[b].skin;
        float dist = glm::length(particles[a].pos - particles[b].pos);
        if (dist <= rSum) {
            // Normalize to (min, max) so pairs from different cells compare
            // equal to the i < j convention used by broad::bruteForce.
            if (a > b) std::swap(a, b);
            pairs.emplace_back(a, b);
        }
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
        return glm::ivec3((int)(x - OFFSET), (int)(y - OFFSET), (int)(z - OFFSET));
    }
};



class Octree {
public:
    // maxDepth guards against pathological recursion (e.g. many coincident
    // particles); leafCapacity is the K referenced in the report (fixed for
    // now, not swept — see Future Work).
    Octree(glm::vec3 center, float halfExtent, int leafCapacity = 8, int maxDepth = 12)
        : leafCapacity_(leafCapacity), maxDepth_(maxDepth) {
        root_ = std::make_unique<Node>();
        root_->center = center;
        root_->halfExtent = halfExtent;
    }
 
    PairList build(const std::vector<Particle>& particles, bool withSkin) {
        // Rebuild from scratch each call, mirroring LinkedCell::build's
        // stateless-per-call contract (Simulation decides *when* to call
        // this via Verlet buffer's needsBuild check; Octree itself doesn't
        // know or care about temporal coherence).
        root_ = std::make_unique<Node>();
        root_->center = rootCenter_;
        root_->halfExtent = rootHalfExtent_;
 
        for (int i = 0; i < (int)particles.size(); ++i) {
            insert(root_.get(), particles, i, 0);
        }
 
        PairList pairs;
        collectPairs(root_.get(), particles, withSkin, pairs);
        return pairs;
    }
 
private:
    struct Node {
        glm::vec3 center{0.0f};
        float halfExtent = 0.0f;
        std::vector<int> indices;                    // populated only on leaves
        std::array<std::unique_ptr<Node>, 8> children; // null until split
        bool isLeaf() const { return children[0] == nullptr; }
    };
 
    int leafCapacity_;
    int maxDepth_;
    glm::vec3 rootCenter_{0.0f};
    float rootHalfExtent_ = 0.0f;
    std::unique_ptr<Node> root_;
 
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
            (childIdx & 4) ? q : -q
        );
        return parentCenter + offset;
    }
 
    void split(Node* node, const std::vector<Particle>& particles, int depth) {
        for (int c = 0; c < 8; ++c) {
            node->children[c] = std::make_unique<Node>();
            node->children[c]->center = childCenter(node->center, node->halfExtent, c);
            node->children[c]->halfExtent = node->halfExtent * 0.5f;
        }
        // Re-insert existing indices into the newly created children;
        // node->indices is cleared since internal nodes don't store particles.
        std::vector<int> existing;
        existing.swap(node->indices);
        for (int idx : existing) {
            insert(node, particles, idx, depth);
        }
    }
 
    void insert(Node* node, const std::vector<Particle>& particles, int idx, int depth) {
        if (node->isLeaf()) {
            node->indices.push_back(idx);
            if ((int)node->indices.size() > leafCapacity_ && depth < maxDepth_) {
                split(node, particles, depth + 1);
            }
            return;
        }
        int c = childIndex(node->center, particles[idx].pos);
        insert(node->children[c].get(), particles, idx, depth + 1);
    }
 
    static void tryAdd(const std::vector<Particle>& particles, int a, int b,
                        bool withSkin, PairList& pairs) {
        float rSum = particles[a].radius + particles[b].radius;
        if (withSkin) rSum += particles[a].skin + particles[b].skin;
        float dist = glm::length(particles[a].pos - particles[b].pos);
        if (dist <= rSum) {
            if (a > b) std::swap(a, b);
            pairs.emplace_back(a, b);
        }
    }
 
    // Correctness-first pair collection: gather all leaves, then test every
    // pair of leaves whose (skin-expanded) bounding boxes could overlap.
    // This is O(L^2) in the number of leaves rather than a proper
    // neighbour-walk, but it is simple to verify against brute force first;
    // optimizing the traversal is left for after correctness is confirmed.
    void collectLeaves(Node* node, std::vector<Node*>& leaves) {
        if (node->isLeaf()) {
            if (!node->indices.empty()) leaves.push_back(node);
            return;
        }
        for (auto& child : node->children) collectLeaves(child.get(), leaves);
    }
 
    static bool boxesOverlap(const Node* a, const Node* b, float margin) {
        glm::vec3 d = glm::abs(a->center - b->center);
        float reach = a->halfExtent + b->halfExtent + margin;
        return d.x <= reach && d.y <= reach && d.z <= reach;
    }
 
    void collectPairs(Node* root, const std::vector<Particle>& particles,
                       bool withSkin, PairList& pairs) {
        std::vector<Node*> leaves;
        collectLeaves(root, leaves);
 
        // Conservative margin so boxesOverlap doesn't reject a leaf pair
        // that could still contain a valid (skin-expanded) collision --
        // largest possible extended radius in the scene, computed on the
        // fly. For correctness-first purposes this is O(n); can be cached
        // by the caller later if it becomes a bottleneck.
        float maxReach = 0.0f;
        if (withSkin) {
            for (const auto& p : particles) maxReach = std::max(maxReach, p.radius + p.skin);
        } else {
            for (const auto& p : particles) maxReach = std::max(maxReach, p.radius);
        }
        float margin = 2.0f * maxReach;
 
        for (size_t i = 0; i < leaves.size(); ++i) {
            // Same-leaf pairs.
            auto& idxA = leaves[i]->indices;
            for (size_t a = 0; a < idxA.size(); ++a)
                for (size_t b = a + 1; b < idxA.size(); ++b)
                    tryAdd(particles, idxA[a], idxA[b], withSkin, pairs);
 
            // Cross-leaf pairs, only for leaf pairs whose boxes could overlap.
            for (size_t j = i + 1; j < leaves.size(); ++j) {
                if (!boxesOverlap(leaves[i], leaves[j], margin)) continue;
                for (int a : leaves[i]->indices)
                    for (int b : leaves[j]->indices)
                        tryAdd(particles, a, b, withSkin, pairs);
            }
        }
    }
};




} // namespace broad
