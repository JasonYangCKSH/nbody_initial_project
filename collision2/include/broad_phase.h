#pragma once
#include <vector>
#include <utility>
#include <algorithm>
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp> 
#include "particle.h"



namespace broad {

class UniformGrid {
private:
    float cellSize_;

    struct ParticleCellEntry {
        uint64_t key;
        int particleIdx;

        bool operator<(const ParticleCellEntry& other) const {
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

    static uint64_t encodeMorton3D(glm::ivec3 cellCoord) {
        constexpr int64_t OFFSET = 1 << 20; 
        uint64_t x = expandBits3D(static_cast<uint64_t>(cellCoord.x + OFFSET));
        uint64_t y = expandBits3D(static_cast<uint64_t>(cellCoord.y + OFFSET));
        uint64_t z = expandBits3D(static_cast<uint64_t>(cellCoord.z + OFFSET));
        
        return (z << 2) | (y << 1) | x;
    }

    glm::ivec3 posToCell(const glm::vec3& pos) const {
        return glm::ivec3(glm::floor(pos / cellSize_));
    }



public:
    explicit UniformGrid(float cellSize) : cellSize_(cellSize) {}

    PairList Build(const std::vector<Particle>& particles, bool withSkin) const {
        // 1. Store particles to cells
        std::vector<ParticleCellEntry> entries;
        entries.reserve(particles.size());
        for (size_t i = 0; i < particles.size(); ++i) {
            glm::ivec3 cell = posToCell(particles[i].pos); 
            uint64_t key = encodeMorton3D(cell);
            entries.push_back({key, static_cast<int>(i)});
        }

        // 使用內部定義的 operator< 進行排序
        std::sort(entries.begin(), entries.end());

        // 2. 從左下到右上 traverse 每個 cell (13 個前向搜尋半導體)
        static const glm::ivec3 kForwardOffsets[13] = {
            {1, 0, 0}, {1, 1, 0}, {0, 1, 0}, {-1, 1, 0},
            {1, 0, -1}, {1, 1, -1}, {0, 1, -1}, {-1, 1, -1},
            {1, 0, 1}, {1, 1, 1}, {0, 1, 1}, {-1, 1, 1},
            {0, 0, 1},
        };

        PairList pairs;
        size_t n = entries.size();
        size_t cellStart = 0;

        while (cellStart < n) {
            uint64_t currentKey = entries[cellStart].key;
            size_t cellEnd = cellStart + 1;
            while (cellEnd < n && entries[cellEnd].key == currentKey) {
                cellEnd++;
            }

            // 2-1 先比對自身 cell 的 particles (內部兩兩比對)
            for (size_t i = cellStart; i < cellEnd; ++i) {
                for (size_t j = i + 1; j < cellEnd; ++j) {
                    int idxA = entries[i].particleIdx;
                    int idxB = entries[j].particleIdx;

                    if (idxA > idxB) std::swap(idxA, idxB);
                    pairs.emplace_back(idxA, idxB);
                }
            }

            // 2-2 再比對鄰居 cell 的 particles
            glm::ivec3 currentCell = posToCell(particles[entries[cellStart].particleIdx].pos);
            for (const auto& offset : kForwardOffsets) {
                glm::ivec3 targetCell = currentCell + offset;
                uint64_t targetKey = encodeMorton3D(targetCell);

                // 修正：使用 Dummy Entry 進行安全的 equal_range 搜尋
                ParticleCellEntry targetDummy{targetKey, 0};
                auto range = std::equal_range(entries.begin(), entries.end(), targetDummy);

                for (size_t i = cellStart; i < cellEnd; ++i) {
                    int idxA = entries[i].particleIdx;
                    for (auto it = range.first; it != range.second; ++it) {
                        int idxB = it->particleIdx;
                        int a = idxA;
                        int b = idxB;
                        if (a > b) std::swap(a, b); // 規範化 pair 索引順序
                        pairs.emplace_back(a, b);    
                    }
                }
            }

            // 移動到下一個 Cell
            cellStart = cellEnd;
        }

        // 3. 回傳 Pair List
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

    void processNode(size_t start, size_t end, int currentDepth,
                     const std::vector<OctreeEntry>& entries,
                     const std::vector<Particle>& particles,
                     bool withSkin, PairList& pairs) const {
        size_t count = end - start;
        if (count <= 1) return;
        if (count <= static_cast<size_t>(leafCapacity_) || currentDepth >= maxDepth_) {
            collideLeaf(start, end, entries, particles, withSkin, pairs);
            return;
        }
        int shift = 3 * (maxDepth_ - 1 - currentDepth);
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
                processNode(childStart, childEnd, currentDepth + 1, entries, particles, withSkin, pairs);
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
            processNode(0, entries.size(), 0, entries, particles, withSkin, pairs);
        }
        return pairs;
    }
};

} // namespace broad