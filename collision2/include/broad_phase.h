#pragma once
#include <vector>
#include <utility>
#include <algorithm>
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp> // 為了使用 glm::distance2

// 假設 Particle 定義於此或外部引入
// #include "particle.h"



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

    // 輔助函式：檢測兩粒子是否碰撞/重疊
    static bool checkOverlap(const Particle& p1, const Particle& p2, bool withSkin) {
        float radiusSum = p1.radius + p2.radius;
        if (withSkin) radiusSum += p1.skin + p2.skin;
        float distSq = glm::distance2(p1.pos, p2.pos);
        return distSq <= (radiusSum * radiusSum);
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
                    if (checkOverlap(particles[idxA], particles[idxB], withSkin)) {
                        if (idxA > idxB) std::swap(idxA, idxB);
                        pairs.emplace_back(idxA, idxB);
                    }
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
                        if (checkOverlap(particles[idxA], particles[idxB], withSkin)) {
                            int a = idxA;
                            int b = idxB;
                            if (a > b) std::swap(a, b); // 規範化 pair 索引順序
                            pairs.emplace_back(a, b);
                        }
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
    struct ParticleOctreeEntry {
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
        uint64_t x = expandBits3D(static_cast<uint64_t>(cellCoord.x));
        uint64_t y = expandBits3D(static_cast<uint64_t>(cellCoord.y));
        uint64_t z = expandBits3D(static_cast<uint64_t>(cellCoord.z));
        return (z << 2) | (y << 1) | x;
    }

    glm::ivec3 posToGrid(const glm::vec3& pos) const {
        // 1. 將空間平移至原點為 (-worldSize_/2) 的正數空間 [0, worldSize_]
        glm::vec3 normalizedPos = pos + glm::vec3(worldSize_ * 0.5f);

        // 2. 計算基礎 Grid 格子尺寸 (總寬度 / 總格數)
        float gridSize = worldSize_ / static_cast<float>(1 << maxDepth_);
        int maxGridIdx = (1 << maxDepth_) - 1;
        // 3. 映射至整數網格座標 [0, 2^maxDepth - 1]
        int gx = std::clamp(static_cast<int>(glm::floor(normalizedPos.x / gridSize)), 0, maxGridIdx);
        int gy = std::clamp(static_cast<int>(glm::floor(normalizedPos.y / gridSize)), 0, maxGridIdx);
        int gz = std::clamp(static_cast<int>(glm::floor(normalizedPos.z / gridSize)), 0, maxGridIdx);

        return glm::ivec3(gx, gy, gz);
    }
    static bool checkOverlap(const Particle& p1, const Particle& p2, bool withSkin) {
        float radiusSum = p1.radius + p2.radius;
        if (withSkin) radiusSum += p1.skin + p2.skin;
        float distSq = glm::distance2(p1.pos, p2.pos);
        return distSq <= (radiusSum * radiusSum);
    }


    void collideLeaf(size_t start, size_t end,
                     const std::vector<OctreeEntry>& entries,
                     const std::vector<Particle>& particles,
                     bool withSkin, PairList& pairs) const {
        for (size_t i = start; i < end; ++i) {
            for (size_t j = i + 1; j < end; ++j) {
                int idxA = entries[i].particleIdx;
                int idxB = entries[j].particleIdx;
                if (checkOverlap(particles[idxA], particles[idxB], withSkin)) {
                    if (idxA > idxB) std::swap(idxA, idxB);
                    pairs.emplace_back(idxA, idxB);
                }
            }
        }
    }

    void processNode(size_t start, size_t end, int currentDepth, const std::vector<ParticleOctreeEntry>& entries, 
                    const std::vector<Particle>& particles, bool withskin, PairList& pairs) const{
        size_t count = end - start;
        if (count <= 1) return;
        if (count <= static_cast<size_t>(leafCapacity_) || currentDepth >= maxDepth_) {
            collideLeaf(start, end, entries, particles, withSkin, pairs);
            return;
        }
        // 2. 計算當前 Depth 下，Octant (0~7) 位於 Morton Key 的位移量
        int shift = 3 * (maxDepth_ - 1 - currentDepth);
        // 3. 遍歷 8 個 Octant (0 ~ 7)，找出各自在 entries 陣列中的子區間 [childStart, childEnd)
        size_t childStart = start;

        for (int octant = 0; octant < 8; ++octant) {
            if (childStart >= end) break; // 已經超出當前區間
         // 尋找第一個「當前層級的 bit 值 > octant」的位置，作為邊界
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
        // 如果這個 Octant 裡面有粒子，遞迴向下處理
            if (childEnd > childStart) {
                processNode(childStart, childEnd, currentDepth + 1, entries, particles, withSkin, pairs);
            }

            // 下一個 Octant 的起點就是上一個 Octant 的終點
            childStart = childEnd;
        }

    }

public:
    Octree(int maxDepth, int leafCapacity, float worldSize):maxDepth_(maxDepth), leafCapacity_(leafCapacity), worldSize_(worldSize){}
    PairList Build(const std::vector<Particle>& particles, bool withSkin) const {
        //1. cell to grid
        std::vector<ParticleOctreeEntry> entries;
        entries.reserve(particles.size());
        for (size_t i = 0; i < particles.size(); ++i) {
            glm::ivec3 cell = posToCell(particles[i].pos); 
            uint64_t key = encodeMorton3D(cell);
            entries.push_back({key, static_cast<int>(i)});
        }
        std::sort(entries.begin(), entries.end());

        PairList pairs;
        if (!entries.empty()) {
            processNode();
        }
        return pairs;
    }

};

} // namespace broad