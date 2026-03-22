#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>
#include <cmath>
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


    NeighborSearch(): method(), cell_size(1.0f){}
    explicit NeighborSearch(NeighborMethod nm, float _cell_size = 1.0f): method(nm), cell_size(_cell_size){}

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
    float cell_size;  // uniform_grid 專用

    // 1. Brute Force
    std::vector<NeighborPair> BruteForce(const std::vector<Body>& bodies) {
        std::vector<NeighborPair> pairs;
        for (int i = 0; i < (int)bodies.size(); i++) {
            for (int j = i + 1; j < (int)bodies.size(); j++) {
                float combinedRadius = bodies[i].radius + bodies[j].radius;

                // 先做 AABB 快速排除
                glm::vec3 d = bodies[j].pos - bodies[i].pos;
                if (std::abs(d.x) > combinedRadius) continue;
                if (std::abs(d.y) > combinedRadius) continue;
                if (std::abs(d.z) > combinedRadius) continue;

                pairs.push_back({i, j});
            }
        }
        return pairs;
    }


     // 2. Uniform Grid
    int HashCell(int x, int y, int z) const {
        return x * 73856093 ^ y * 19349663 ^ z * 83492791;
    }
    glm::ivec3 BodyToCell(int pos_x, int pos_y, int pos_z) const {
        return {
            (int)std::floor(pos_x / cell_size),
            (int)std::floor(pos_y / cell_size),
            (int)std::floor(pos_z / cell_size)
        };
    }
    std::vector<NeighborPair> UniformGrid(const std::vector<Body>& bodies) {
        std::vector<NeighborPair> pairs;
        std::unordered_map<int, std::vector<int>> grid;

        // 1. Build Cell, insert every Body into its corresponding cell
        for (int i = 0; i < (int)bodies.size(); i++) {
            glm::ivec3 cell = BodyToCell(bodies[i].pos.x, bodies[i].pos.y, bodies[i].pos.z);
            grid[HashCell(cell.x, cell.y, cell.z)].push_back(i);
        }

        // 2. Query: for every Body, search its neighbor 27 cells
        for (int i = 0; i < (int)bodies.size(); i++) {
            glm::ivec3 cell = BodyToCell(bodies[i].pos.x, bodies[i].pos.y, bodies[i].pos.z);
            for (int dx = -1; dx <= 1; dx++)
                for (int dy = -1; dy <= 1; dy++)
                    for (int dz = -1; dz <= 1; dz++) {
                        glm::ivec3 key = BodyToCell(cell.x + dx, cell.y + dy, cell.z + dz);
                        auto it = grid.find(HashCell(key.x, key.y, key.z)); 
                        if (it == grid.end()) continue;
                        for (int j : it->second) {
                            if (i >= j) continue;
                            float combinedRadius = bodies[i].radius + bodies[j].radius;
                            glm::vec3 d = bodies[j].pos - bodies[i].pos;
                            if (std::abs(d.x) > combinedRadius) continue;
                            if (std::abs(d.y) > combinedRadius) continue;
                            if (std::abs(d.z) > combinedRadius) continue;        
                            pairs.push_back({i, j});       
                        } 
                    } 
        }
        return pairs;
    }

    // 3. Octree
    std::vector<NeighborPair> OctreeSearch(const std::vector<Body>& bodies) {
        NeighborSearchOctree nsOctree;
        
    }

};
