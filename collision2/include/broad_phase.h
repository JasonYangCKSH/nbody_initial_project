#pragma once
#include <vector>
#include <utility>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cmath>
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


    mutable int cachedCellRadius_ = -1;
    mutable std::vector<glm::ivec3> cachedForwardOffsets_;

    const std::vector<glm::ivec3>& forwardOffsetsFor(int cellRadius) const {
        if (cellRadius != cachedCellRadius_) {
            cachedForwardOffsets_.clear();
            for (int dz = -cellRadius; dz <= cellRadius; ++dz) {
                for (int dy = -cellRadius; dy <= cellRadius; ++dy) {
                    for (int dx = -cellRadius; dx <= cellRadius; ++dx) {
                        if (dx == 0 && dy == 0 && dz == 0) continue;
                        bool forward = (dy > 0) || (dy == 0 && dx > 0) ||
                                       (dy == 0 && dx == 0 && dz > 0);
                        if (forward) cachedForwardOffsets_.push_back({dx, dy, dz});
                    }
                }
            }
            cachedCellRadius_ = cellRadius;
        }
        return cachedForwardOffsets_;
    }

public:
    explicit UniformGrid(float cellSize) : cellSize_(cellSize) {}

    PairList Build(const std::vector<Particle>& particles, bool withSkin) const {
        // 1. Store particles to cells
        std::vector<ParticleCellEntry> entries;
        entries.reserve(particles.size());
        float maxReach = 0.0f;
        for (size_t i = 0; i < particles.size(); ++i) {
            glm::ivec3 cell = posToCell(particles[i].pos);
            uint64_t key = encodeMorton3D(cell);
            entries.push_back({key, static_cast<int>(i)});
            maxReach = std::max(maxReach, particles[i].radius + (withSkin ? particles[i].skin : 0.0f));
        }

        // 使用內部定義的 operator< 進行排序
        std::sort(entries.begin(), entries.end());

    
        int cellRadius = std::max(
            1, static_cast<int>(std::ceil((2.0f * maxReach) / cellSize_)) + 1);

        PairList pairs;
        size_t n = entries.size();

        // 2-1. Group entries into per-cell runs, and emit intra-cell pairs
        // as we go (this part is unavoidable and cheap either way).
        struct CellGroup {
            glm::ivec3 coord;
            size_t start, end;
        };
        std::vector<CellGroup> groups;
        {
            size_t cellStart = 0;
            while (cellStart < n) {
                uint64_t currentKey = entries[cellStart].key;
                size_t cellEnd = cellStart + 1;
                while (cellEnd < n && entries[cellEnd].key == currentKey) {
                    cellEnd++;
                }
                for (size_t i = cellStart; i < cellEnd; ++i) {
                    for (size_t j = i + 1; j < cellEnd; ++j) {
                        int idxA = entries[i].particleIdx;
                        int idxB = entries[j].particleIdx;
                        if (idxA > idxB) std::swap(idxA, idxB);
                        pairs.emplace_back(idxA, idxB);
                    }
                }
                groups.push_back({posToCell(particles[entries[cellStart].particleIdx].pos),
                                   cellStart, cellEnd});
                cellStart = cellEnd;
            }
        }


        size_t numGroups = groups.size();
        long long cubeSide = 2LL * cellRadius + 1;
        long long forwardOffsetCount = (cubeSide * cubeSide * cubeSide - 1) / 2;

        if (static_cast<long long>(numGroups) <= forwardOffsetCount) {
            for (size_t a = 0; a < numGroups; ++a) {
                for (size_t b = a + 1; b < numGroups; ++b) {
                    glm::ivec3 d = groups[a].coord - groups[b].coord;
                    if (std::abs(d.x) <= cellRadius && std::abs(d.y) <= cellRadius &&
                        std::abs(d.z) <= cellRadius) {
                        for (size_t i = groups[a].start; i < groups[a].end; ++i) {
                            int idxA = entries[i].particleIdx;
                            for (size_t j = groups[b].start; j < groups[b].end; ++j) {
                                int idxB = entries[j].particleIdx;
                                int x = idxA, y = idxB;
                                if (x > y) std::swap(x, y);
                                pairs.emplace_back(x, y);
                            }
                        }
                    }
                }
            }
        } else {
            const std::vector<glm::ivec3>& forwardOffsets = forwardOffsetsFor(cellRadius);
            for (const auto& g : groups) {
                for (const auto& offset : forwardOffsets) {
                    glm::ivec3 targetCell = g.coord + offset;
                    uint64_t targetKey = encodeMorton3D(targetCell);

                    ParticleCellEntry targetDummy{targetKey, 0};
                    auto range = std::equal_range(entries.begin(), entries.end(), targetDummy);

                    for (size_t i = g.start; i < g.end; ++i) {
                        int idxA = entries[i].particleIdx;
                        for (auto it = range.first; it != range.second; ++it) {
                            int idxB = it->particleIdx;
                            int a = idxA, b = idxB;
                            if (a > b) std::swap(a, b);
                            pairs.emplace_back(a, b);
                        }
                    }
                }
            }
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