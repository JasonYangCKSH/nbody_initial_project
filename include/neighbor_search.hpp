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


};
#endif 