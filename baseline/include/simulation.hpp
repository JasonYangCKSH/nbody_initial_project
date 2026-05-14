#pragma once
#include <vector>
#include <iostream>
#include <glm/glm.hpp>
#include <utility>
#include <unordered_map>
#include "body.hpp"

struct NeighborPair {
    int i, j; // 2 bodies' index
    float distance;
};

class Simulation {
private:
    std::vector<Body> bodies;
    float searchRadius;
public:
    Simulation(float _searchRadius): searchRadius(_searchRadius){}
    // ---- A. Test Bench Generation ----
    void GenerateRandom(int n, float rangeMin, float rangeMax);
    void GenerateUniform(int nx, int ny, int nz, float spacing);
    void GenerateSimple();
    // ---- B. Finding Neighbor Methods ----
    std::vector<NeighborPair> BruteForce();
    std::vector<NeighborPair> UniformGrid();
    std::vector<NeighborPair> Octree();

    // ---- C. Application ----
    const std::vector<Body>& GetBodies() const { return bodies; }
    int GetBodiesSize() const { return bodies.size();}
    void PrintPairsResult(const std::vector<NeighborPair>& pairs) const;


};

std::vector<NeighborPair> Simulation::BruteForce() {

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