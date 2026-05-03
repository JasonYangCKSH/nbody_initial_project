#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include "body.hpp"
#include "NeighborSearchOctree.hpp"
enum class NeighborMethod {
    BRUTE_FORCE,
    UNIFORM_GRID,
    OCTREE

};

struct NeighborPair {
    int i, j;
};
class NeighborSearch {
public:


    NeighborSearch(): method(){}
    explicit NeighborSearch(NeighborMethod nm): method(nm){}

    std::vector<NeighborPair> FindPairs(const std::vector<Body>& bodies) {
        switch (method) {
            case(NeighborMethod::BRUTE_FORCE):  return BruteForce(bodies);
            case(NeighborMethod::UNIFORM_GRID): return UniformGrid(bodies);
            case(NeighborMethod::OCTREE): return OctreeSearch(bodies);
            default:    return {};
        }
    }

private:
    NeighborMethod method;

    // 1. Brute Force
    std::vector<NeighborPair> BruteForce(const std::vector<Body>& bodies) {
        if (bodies.empty()) return {};
        std::vector<NeighborPair> pairs;
        for (int i = 0; i < (int)bodies.size(); i++) {
            for (int j = i + 1; j < (int)bodies.size(); j++) {
                float combinedRadius = bodies[i].radius + bodies[j].radius;
                glm::vec3 d = bodies[j].pos - bodies[i].pos;
                float dist2 = d.x*d.x + d.y*d.y + d.z*d.z;
                if (dist2 > combinedRadius * combinedRadius) continue;
                pairs.push_back({i, j});
            }
        }
        return pairs;
    }

    int HashCoord(int cellx, int celly, int cellz, int numofcells) {
        unsigned int h = ((unsigned int)cellx * 92837111u)
                    ^ ((unsigned int)celly * 689287499u)
                    ^ ((unsigned int)cellz * 283923481u);
        return (int)(h % (unsigned int)numofcells);
    }

    // 2. Uniform Grid
    std::vector<NeighborPair> UniformGrid(const std::vector<Body>& bodies) {
        
        // [cell_size] is important
        if (bodies.empty()) return {};
        std::vector<NeighborPair> pairs;
        // 1. initialize cellsize
        float cellsize = 0.0f;
        float maxRadius = 0.0f;
        float x_min = bodies[0].pos.x, x_max = bodies[0].pos.x;
        float y_min = bodies[0].pos.y, y_max = bodies[0].pos.y;
        float z_min = bodies[0].pos.z, z_max = bodies[0].pos.z;
        for (const auto& b: bodies) {
            x_min = std::min(x_min, b.pos.x);
            y_min = std::min(y_min, b.pos.y);
            z_min = std::min(z_min, b.pos.z);
            x_max = std::max(x_max, b.pos.x);
            y_max = std::max(y_max, b.pos.y);
            z_max = std::max(z_max, b.pos.z);

            maxRadius = std::max(maxRadius, b.radius);
            cellsize = std::max(cellsize, 2 * b.radius);
        }
        if (cellsize <= 0.0f) {
            throw std::runtime_error("Invalid cell size.");
        }
        
    
        cellsize = std::max(cellsize, 2.0f * maxRadius); 

    
        int hashTableSize = static_cast<int>(bodies.size()) * 2;
        assert(bodies.size() < (size_t)INT_MAX / 2);


        std::vector<int> hashTable(hashTableSize);
        std::fill(hashTable.begin(), hashTable.end(), 0);

        // Step 1: Count particles per hashed cell
        for (size_t i = 0; i < static_cast<size_t> (bodies.size()); i++) {
            int cx = static_cast<int>(std::floor((bodies[i].pos.x - x_min) / cellsize));
            int cy = static_cast<int>(std::floor((bodies[i].pos.y - y_min) / cellsize));
            int cz = static_cast<int>(std::floor((bodies[i].pos.z - z_min) / cellsize));

            int hash = HashCoord(cx, cy, cz, hashTableSize);

            hashTable[hash]++;
        }

        // Step2: prefix sum
        int start = 0;
        for (int i = 0; i < hashTableSize; i++) {
            int count = hashTable[i];
            hashTable[i] = start;
            start += count;
        }
        
        // Step 3: Fill particle map + store exact cell coordinates
        std::vector<int> particleMap(bodies.size());
        std::vector<int> currentOffset = hashTable;

        std::vector<int> particleCellX(bodies.size());
        std::vector<int> particleCellY(bodies.size());
        std::vector<int> particleCellZ(bodies.size());

        for (size_t i = 0; i < bodies.size(); i++) {
            int cx = static_cast<int>(std::floor((bodies[i].pos.x - x_min) / cellsize));
            int cy = static_cast<int>(std::floor((bodies[i].pos.y - y_min) / cellsize));
            int cz = static_cast<int>(std::floor((bodies[i].pos.z - z_min) / cellsize));

            int hash = HashCoord(cx, cy, cz, hashTableSize);

            particleMap[currentOffset[hash]] = static_cast<int>(i);
            particleCellX[i] = cx;
            particleCellY[i] = cy;
            particleCellZ[i] = cz;

            currentOffset[hash]++;
        }

        // Step 4: Neighbor search
        for (size_t i = 0; i < bodies.size(); i++) {
            int cx = particleCellX[i];
            int cy = particleCellY[i];
            int cz = particleCellZ[i];

            for (int dx = -1; dx <= 1; dx++)
            for (int dy = -1; dy <= 1; dy++)
            for (int dz = -1; dz <= 1; dz++) {

                int ncx = cx + dx;
                int ncy = cy + dy;
                int ncz = cz + dz;

                int neighborHash = HashCoord(ncx, ncy, ncz, hashTableSize);

                int startIndex = hashTable[neighborHash];
                // Fixed: safely calculate endIndex by finding next non-empty bucket
                int endIndex = static_cast<int>(bodies.size());
                for (int k = neighborHash + 1; k < hashTableSize; k++) {
                    if (hashTable[k] > startIndex) {
                        endIndex = hashTable[k];
                        break;
                    }
                }

                for (int k = startIndex; k < endIndex; k++) {
                    int j = particleMap[k];

                    if (j <= static_cast<int>(i)) continue;

                    // Verify exact cell match (avoid hash collisions)
                    if (particleCellX[j] != ncx ||
                        particleCellY[j] != ncy ||
                        particleCellZ[j] != ncz) {
                        continue;
                    }

                    float combinedRadius = bodies[i].radius + bodies[j].radius;
                    glm::vec3 d = bodies[j].pos - bodies[i].pos;
                    float dist2 = d.x * d.x + d.y * d.y + d.z * d.z;

                    if (dist2 <= combinedRadius * combinedRadius) {
                        pairs.push_back({ static_cast<int>(i), j });
                    }
                }
            }
        }
        return pairs;
    }

    // 3. Octree
    std::vector<NeighborPair> OctreeSearch(const std::vector<Body>& bodies) {
        std::vector<NeighborPair> pairs;
        NSOctree nsOctree;
        // 1.rebuild an octree
        nsOctree.Build(bodies);
        // 2. search neighbor
        std::vector<int> neighbors;
        for (int i = 0; i < (int)bodies.size(); i++) {
            neighbors.clear();
            // 
            nsOctree.Query(i, bodies[i].radius, neighbors);
            for (int j : neighbors)
                if (i < j) {
                    pairs.push_back({i, j});
                    //std::cout <<"Octree: ("<< i << ", " << j <<")"<< std::endl;
                }
        }
        return pairs;
    }

};
