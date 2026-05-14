#pragma once
#include <vector>
#include <iostream>
#include <glm/glm.hpp>
#include <utility>
#include <unordered_map>
#include "body.hpp"

struct NeighborPair {
    int i, j; // 2 bodies' index
    float distance2;
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

