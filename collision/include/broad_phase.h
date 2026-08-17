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

} // namespace broad
