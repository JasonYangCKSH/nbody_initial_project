#ifndef NEIGHBOR_SEARCH_HPP
#define NEIGHBOR_SEARCH_HPP
#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>
#include <cmath>
#include "body.hpp"
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


    NeighborSearch(): method(NeighborMethod::NONE), cell_size(1.0f){}
    explicit NeighborSearch(NeighborMethod nm, float _cell_size = 1.0f): method(nm), cell_size(_cell_size){}

    std::vector<NeighborPair> FindPairs(const std::vector<Body>& bodies) {
        switch (method) {
            case(NeighborMetHod::BRUTE_FORCE):  return BruteForce(bodies);
            case(NeighborMetHod::UNIFORM_GRID): return UniformGrid(bodies);
            case(NeighborMetHod::OCTREE): return OctreeSearch();
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
            for (int j = i + 1; j < n; j++) {
                float combinedRadius = bodies[i].radius + bodies[j].radius;

                // 先做 AABB 快速排除
                glm::vec3 d = bodies[j].pos - bodies[i].pos;
                if (std::abs(d.x) > combined_r) continue;
                if (std::abs(d.y) > combined_r) continue;
                if (std::abs(d.z) > combined_r) continue;

                pairs.push_back({i, j});
            }
        }
        return pairs;
    }


     // 2. Uniform Grid
    int HashCell(int x, int y, int z) const {
        return x * 73856093 ^ y * 19349663 ^ z * 83492791;
    }
    glm::ivec3 BodyToCell(const glm::vec3& pos) const {
        return {
            (int)std::floor(pos.x / cell_size),
            (int)std::floor(pos.y / cell_size),
            (int)std::floor(pos.z / cell_size)
        };
    }
    std::vector<NeighborSearch> UniformGrid(const std::vector<Body>& bodies) {
        std::vector<NeighborPair> pairs;
        std::unordered_map<int, std::vector<int>> grid;

        // 1. Build Cell, insert every Body into its corresponding cell
        for (int i = 0; i < (int)bodies.size(); i++) {
            glm::ivec3 cell = world_to_cell(bodies[i].pos);
            grid[HashCell(cell.x, cell.y, cell.z)].push_back(i);
        }

        // 2. Query: for every Body, search its neighbor 27 cells
        for (int i = 0; i < (int)bodies.size(); i++) {
            glm::ivec3 cell = BodyToCell(bodies[i].pos);
            for (int dx = -1; dx <= 1; dx++)
                for (int dy = -1; dy <= 1; dy++)
                    for (int dz = -1; dz <= 1; dz++) {
                        glm::ivec3 key = BodyToCell(cell.x + dx, cell.y + dy, cell.z + dz);
                        auto it = grid.find(key);
                        if (it == grid.end()) continue;
                        for (int j : it->second) {
                            if (i >= j) continue;
                            float combinedRadius = body[i].radius + body[j].radius;
                            glm::vec3 d = bodies[j].pos - bodies[i].pos;
                            if (std::abs(d.x) > combined_r) continue;
                            if (std::abs(d.y) > combined_r) continue;
                            if (std::abs(d.z) > combined_r) continue;        
                            pairs.push_back({i, j});       
                        } 
                    } 
        }
        return pairs;
    }

    // 3. Octree
    std::vector<NeighborSearch> OctreeSearch(const std::vector<Body>& bodies) {
        return BruteForce(bodies);
    }

};
#endif 