
#include "simulation.h"
#include "scenario.h"

#include <iostream>
#include <string>
#include <vector>

const float particleRadius = 1.0;
const float cellSize = 2.0;
const int maxDepth = 10;
const int leafCapacity = 16;
const float worldSize = 100.0f;
const bool hasSkin = true;
const float K = 10.0f;
const float dt = 1.0f/60.0f;

const Method Method1 = Method::BruteForce;
const Method Method2 = Method::UniformGrid;
const int totalFrame = 2;


int main() {
    // 實作SimulationConfig for brute force
    SimulationConfig cfg1(particleRadius, dt, K, hasSkin, Method1, cellSize, maxDepth, leafCapacity, worldSize);
    Simulation sim1(cfg1);

    // 實作SimulationConfig and Simulation for uniform grid
    SimulationConfig cfg2(particleRadius, dt, K, hasSkin, Method2, cellSize, maxDepth, leafCapacity, worldSize);
    Simulation sim2(cfg2);

    auto particles = scenario::spatialCluster(20000, worldSize, particleRadius, 1.0f, 0.0f,
                                            1.0,
                                            0.03,
                                            1);
    sim1.initialize(particles, totalFrame);
    sim2.initialize(particles, totalFrame);
    
    sim1.run();
    sim2.run();

    // compare method2 result to method 1, check if the result is correct
    bool ok = true;
    for (int i = 0; i < sim1.totalFrames() && ok; ++i)
        ok = sim1.frameHistory()[i].collisionPairs == sim2.frameHistory()[i].collisionPairs;

    std::cout << (ok ? "PASS" : "FAIL") << std::endl;
}
