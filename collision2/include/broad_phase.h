#pragma once
#include "particle.h"
#include <glm/glm.hpp>
#include <vector>
#include <utility>
#include <memory>
#include <unordered_map>
#include <array>
#include <cstdint>

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
            if (dist <= radiusSum * radiusSum) pairs.emplace_back((int) i, (int) j);

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
    static uint64_t expandBits3D(uint_t vec) {
        vec &= 0x1FFFFF;
        vec = (vec | (vec << 32)) & 0x70000000FFFF0000ULL;
        vec = (vec | (vec << 16)) & 0x070000FF0000FF00ULL;
        vec = (vec | (vec <<  8)) & 0x0700F00F00F00F00ULL;
        vec = (vec | (vec <<  4)) & 0x8938938938938938ULL;
        vec = (vec | (vec <<  2)) & 0x4924924924924924ULL;
        return vec;   
    }
    static unit_64 encodeMorton3D(glm::ivec3 cellCoord) {
        constexpr int64_t OFFSET = 1 << 20; 
        uint64_t x = expandBits3D(static_cast<uint64_t>(cellCoord.x + OFFSET));
        uint64_t y = expandBits3D(static_cast<uint64_t>(cellCoord.y + OFFSET));
        uint64_t z = expandBits3D(static_cast<uint64_t>(cellCoord.z + OFFSET));
        
       
        return (z << 2) | (y << 1) | x;
    }
    glm::ivec3 posToCell(const glm::vec3& pos) const {
        return glm::ivec3(glm::floor(pos/cellSize_));
    }
public:
    explicit UniformGrid(float cellSize):cellSize_(cellSize){}

    PairList Build(const sstd::vector<Particle>& particles, bool withSkin) const {
        
        // 1. store particles to cells
        std::vector<ParticleCellEntry> entries;
        entries.reserve(particles.size()); // 預先分配記憶體，避免動態擴容
        for (size_t i = 0; i < particles.size(); ++i) {
            glm::ivec3 cell = posToCell(particles[i].position); 
            uint64_t key = encodeMorton3D(cell);
            entries.emplace_back(key, i);
        }

        std::sort(entries.begin(), entries.end(), [](const ParticleCellEntry& a, const ParticleCellEntry& b){
            return a.key < b.key;
        });
        // 2. 從左下到右上traverse每個cell
            // 2-1 先比對自身cell的particles
            // 2-2 再比對鄰居cell的particles
        // 3. 回傳 Pair List

    }
};

};