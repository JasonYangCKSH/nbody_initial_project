#include <iostream>
#include <chrono>
#include "simulation.hpp"

int main() {
    Simulation sim(1.0f); // searchRadius = 1.0 = 2 * body_radius, which is something to do with body's radius(0.5)
    sim.GenerateSimple();
    // case1: brute force
    auto start = std::chrono::high_resolution_clock::now();
    auto pairs = sim.BruteForce();
    auto end = std::chrono::high_resolution_clock::now();
    sim.PrintPairsResult(pairs);
    std::chrono::duration<double, std::milli> elapsed = end - start;
    std::cout << "Time Spend: " << elapsed.count() << " ms\n";
    
    // case2: uniform grid



    // case3: octree
    
    
    return 0;
}