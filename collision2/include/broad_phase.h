#pragma once
#include <vector>
#include <utility>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>
#include "particle.h"



namespace broad {

// Linked-cell 版本的 uniform grid：假設 cellSize 已經大到能容納最大的
// extended bounding sphere（radius + skin，見 verlet::capSkinToCellSize），
// 因此只需要走訪同格 + 13 個「前向」鄰居格（half-shell stencil）就能
// 涵蓋每一對可能相互作用的粒子，不用像過去 Morton 排序那樣依賴可變的
// cellRadius。
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
        (void)withSkin;  // 半徑固定為 1 格，withSkin 只保留給呼叫端介面一致

        std::unordered_map<int64_t, std::vector<int>> cells;
        cells.reserve(particles.size());
        for (size_t i = 0; i < particles.size(); ++i) {
            cells[key(posToCell(particles[i].pos))].push_back(static_cast<int>(i));
        }

        // 13 個「前向」鄰居 offset，加上同一格自己，恰好走訪每一對格子一次，
        // 等同過去用 cellA 座標 < cellB 座標去重的規則。
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
                    int x = cellA[a], y = cellA[b];
                    if (x > y) std::swap(x, y);
                    pairs.emplace_back(x, y);
                }
            }

            for (const auto& offset : kForwardOffsets) {
                auto it = cells.find(key(base + offset));
                if (it == cells.end()) continue;
                for (int a : cellA) {
                    for (int b : it->second) {
                        int x = a, y = b;
                        if (x > y) std::swap(x, y);
                        pairs.emplace_back(x, y);
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

    struct OctreeEntry {
        uint64_t key;
        int particleIdx;

        bool operator<(const OctreeEntry& other) const {
            return key < other.key;
        }
    };

    static uint64_t expandBits3D(uint64_t vec) {
        vec &= 0x1FFFFFULL;
        vec = (vec | (vec << 32)) & 0x1F00000000FFFFULL;
        vec = (vec | (vec << 16)) & 0x1F0000FF0000FFULL;
        vec = (vec | (vec <<  8)) & 0x100F00F00F00F00FULL;
        vec = (vec | (vec <<  4)) & 0x10C30C30C30C30C3ULL;
        vec = (vec | (vec <<  2)) & 0x1249249249249249ULL;
        return vec;
    }

    static uint64_t encodeMorton3D(glm::ivec3 gridCoord) {
        uint64_t x = expandBits3D(static_cast<uint64_t>(gridCoord.x));
        uint64_t y = expandBits3D(static_cast<uint64_t>(gridCoord.y));
        uint64_t z = expandBits3D(static_cast<uint64_t>(gridCoord.z));
        return (z << 2) | (y << 1) | x;
    }

    glm::ivec3 posToGrid(const glm::vec3& pos) const {
        glm::vec3 normalizedPos = pos + glm::vec3(worldSize_ * 0.5f);
        float gridSize = worldSize_ / static_cast<float>(1 << maxDepth_);
        int maxGridIdx = (1 << maxDepth_) - 1;

        int gx = std::clamp(static_cast<int>(glm::floor(normalizedPos.x / gridSize)), 0, maxGridIdx);
        int gy = std::clamp(static_cast<int>(glm::floor(normalizedPos.y / gridSize)), 0, maxGridIdx);
        int gz = std::clamp(static_cast<int>(glm::floor(normalizedPos.z / gridSize)), 0, maxGridIdx);

        return glm::ivec3(gx, gy, gz);
    }



    void collideLeaf(size_t start, size_t end,
                     const std::vector<OctreeEntry>& entries,
                     const std::vector<Particle>& particles,
                     bool withSkin, PairList& pairs) const {
        for (size_t i = start; i < end; ++i) {
            for (size_t j = i + 1; j < end; ++j) {
                int idxA = entries[i].particleIdx;
                int idxB = entries[j].particleIdx;
                if (idxA > idxB) std::swap(idxA, idxB);
                pairs.emplace_back(idxA, idxB);

            }
        }
    }

    bool canSplit(size_t start, size_t end,
                  const std::vector<OctreeEntry>& entries,
                  const std::vector<Particle>& particles,
                  bool withSkin, const glm::vec3& nodeMin, float nodeSize) const {
        glm::vec3 mid = nodeMin + glm::vec3(nodeSize * 0.5f);
        for (size_t i = start; i < end; ++i) {
            const Particle& p = particles[entries[i].particleIdx];
            float reach = p.radius + (withSkin ? p.skin : 0.0f);
            glm::vec3 d = glm::abs(p.pos - mid);
            if (d.x <= reach || d.y <= reach || d.z <= reach) return false;
        }
        return true;
    }

    void processNode(size_t start, size_t end, int currentDepth,
                     const std::vector<OctreeEntry>& entries,
                     const std::vector<Particle>& particles,
                     bool withSkin, const glm::vec3& nodeMin, float nodeSize,
                     PairList& pairs) const {
        size_t count = end - start;
        if (count <= 1) return;
        bool atCapacity = count <= static_cast<size_t>(leafCapacity_) || currentDepth >= maxDepth_;
        if (atCapacity || !canSplit(start, end, entries, particles, withSkin, nodeMin, nodeSize)) {
            collideLeaf(start, end, entries, particles, withSkin, pairs);
            return;
        }
        int shift = 3 * (maxDepth_ - 1 - currentDepth);
        float childSize = nodeSize * 0.5f;
        size_t childStart = start;
        for (int octant = 0; octant < 8; ++octant) {
            if (childStart >= end) break;
            auto it = std::lower_bound(
                entries.begin() + childStart,
                entries.begin() + end,
                octant + 1,
                [shift](const OctreeEntry& entry, int targetOctant) {
                    int currentOctant = static_cast<int>((entry.key >> shift) & 7ULL);
                    return currentOctant < targetOctant;
                }
            );
            size_t childEnd = std::distance(entries.begin(), it);
            if (childEnd > childStart) {
                glm::vec3 childMin = nodeMin + glm::vec3(
                    (octant & 1) ? childSize : 0.0f,
                    (octant & 2) ? childSize : 0.0f,
                    (octant & 4) ? childSize : 0.0f);
                processNode(childStart, childEnd, currentDepth + 1, entries, particles,
                            withSkin, childMin, childSize, pairs);
            }
            childStart = childEnd;
        }
    }

public:
    Octree(int maxDepth, int leafCapacity, float worldSize)
        : maxDepth_(maxDepth), leafCapacity_(leafCapacity), worldSize_(worldSize) {}

    PairList Build(const std::vector<Particle>& particles, bool withSkin) const {
        std::vector<OctreeEntry> entries;
        entries.reserve(particles.size());

        for (size_t i = 0; i < particles.size(); ++i) {
            glm::ivec3 grid = posToGrid(particles[i].pos);
            uint64_t key = encodeMorton3D(grid);
            entries.push_back({key, static_cast<int>(i)});
        }

        std::sort(entries.begin(), entries.end());

        PairList pairs;
        if (!entries.empty()) {
            glm::vec3 rootMin(-worldSize_ * 0.5f);
            processNode(0, entries.size(), 0, entries, particles, withSkin, rootMin, worldSize_, pairs);
        }
        return pairs;
    }
};

} // namespace broad