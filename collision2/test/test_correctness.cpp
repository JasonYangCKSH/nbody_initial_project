
#include "simulation.h"
#include "scenario.h"

#include <iostream>
#include <string>
#include <vector>

const float particleRadius = 1.0;
const float cellSize = 2.0;
const int maxDepth = 10;
const int leafCapacity = 50;
const float worldSize = 100.0f;
const bool hasSkin = false;
const float K = 100.0f;
const float dt = 1.0f/60.0f;

const Method Method1 = Method::BruteForce;
const Method Method2 = Method::Octree;
const int totalFrame = 20;


int main() {
    // 實作SimulationConfig for brute force
    SimulationConfig cfg1(particleRadius, dt, K, hasSkin, Method1, cellSize, maxDepth, leafCapacity, worldSize);
    Simulation sim1(cfg1);

    // 實作SimulationConfig and Simulation for uniform grid
    SimulationConfig cfg2(particleRadius, dt, K, hasSkin, Method2, cellSize, maxDepth, leafCapacity, worldSize);
    Simulation sim2(cfg2);

    auto particles = scenario::spatialCluster(2000, worldSize, particleRadius, 10.0f, 0.0f,
                                            0.0,
                                            0.03,
                                            1);
    sim1.initialize(particles, totalFrame);
    sim2.initialize(particles, totalFrame);
    auto t1 = std::chrono::steady_clock::now();
    sim1.run();
    auto t2 = std::chrono::steady_clock::now();
    sim2.run();
    auto t3 = std::chrono::steady_clock::now();

    std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << "\n";
    std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count() << "\n";
    // compare method2 result to method 1, check if the result is correct
    bool ok = true;
    
    for (int i = 0; i < sim1.totalFrames() && ok; ++i) {
        ok = sim1.frameHistory()[i].collisionPairs == sim2.frameHistory()[i].collisionPairs;
        
    }
    
    std::cout << (ok ? "PASS" : "FAIL") << std::endl;
}
