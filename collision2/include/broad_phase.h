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

        // 重載 < 運算子，方便 std::sort 與 std::equal_range 直接使用
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

public:

};

} // namespace broad