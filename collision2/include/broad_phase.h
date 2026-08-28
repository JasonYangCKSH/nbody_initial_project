#pragma once
#include <vector>
#include <utility>
#include <algorithm>
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp> // 為了使用 glm::distance2



using PairList = std::vector<std::pair<int, int>>;

namespace broad {

inline PairList BruteForce(const std::vector<Particle>& particles, bool withSkin) {
    PairList pairs;
    const size_t n = particles.size();
    for (size_t i = 0; i < particles.size(); ++i) {
        for (size_t j = i + 1; j < n; ++j) {
            float radiusSum = particles[i].radius + particles[j].radius;
            if (withSkin) radiusSum += particles[i].skin + particles[j].skin;
            float dist = glm::distance2(particles[i].pos, particles[j].pos); 
            if (dist <= radiusSum * radiusSum) pairs.emplace_back((int)i, (int)j);
        }
    }
    return pairs;
}

class UniformGrid {
private:
    float cellSize_;

    struct ParticleCellEntry {
        uint64_t key;
        int particleIdx;
    };

    static uint64_t expandBits3D(uint64_t vec) {
        vec &= 0x1FFFFF;
        vec = (vec | (vec << 32)) & 0x70000000FFFF0000ULL;
        vec = (vec | (vec << 16)) & 0x070000FF0000FF00ULL;
        vec = (vec | (vec <<  8)) & 0x0700F00F00F00F00ULL;
        vec = (vec | (vec <<  4)) & 0x8938938938938938ULL;
        vec = (vec | (vec <<  2)) & 0x4924924924924924ULL;
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
            entries.emplace_back(ParticleCellEntry{key, static_cast<int>(i)});
        }

        std::sort(entries.begin(), entries.end(), [](const ParticleCellEntry& a, const ParticleCellEntry& b){
            return a.key < b.key;
        });

        // 2. 從左下到右上 traverse 每個 cell
        static const glm::ivec3 kForwardOffsets[13] = {
            {1, 0, 0}, {1, 1, 0}, {0, 1, 0}, {-1, 1, 0},
            {1, 0, -1}, {1, 1, -1}, {0, 1, -1}, {-1, 1, -1},
            {1, 0, 1}, {1, 1, 1}, {0, 1, 1}, {-1, 1, 1},
            {0, 0, 1},
        };

        PairList pairs;
        size_t n = entries.size();
        size_t cellStart = 0;

        auto keyCompare = [](const ParticleCellEntry& entry, uint64_t val) {
            return entry.key < val;
        };

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
                        pairs.emplace_back(idxA, idxB);
                    }
                }
            }

            // 2-2 再比對鄰居 cell 的 particles
            glm::ivec3 currentCell = posToCell(particles[entries[cellStart].particleIdx].pos);
            for (const auto& offset : kForwardOffsets) {
                glm::ivec3 targetCell = currentCell + offset;
                uint64_t targetKey = encodeMorton3D(targetCell);

                // 用二分搜尋快速定位目標鄰居 Cell 在 entries 中的範圍
                auto range = std::equal_range(entries.begin(), entries.end(), targetKey, keyCompare);

                for (size_t i = cellStart; i < cellEnd; ++i) {
                    int idxA = entries[i].particleIdx;
                    for (auto it = range.first; it != range.second; ++it) {
                        int idxB = it->particleIdx;
                        if (checkOverlap(particles[idxA], particles[idxB], withSkin)) {
                            pairs.emplace_back(idxA, idxB);
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