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
    std::cout << "\nBrute Force Result:\n";
    sim.PrintPairsResult(pairs);
    std::chrono::duration<double, std::milli> elapsed = end - start;
    std::cout << "Time Spend: " << elapsed.count() << " ms\n";
    
    // case2: uniform grid
    auto start2 = std::chrono::high_resolution_clock::now();
    auto pairs2 = sim.UniformGrid();
    auto end2 = std::chrono::high_resolution_clock::now();
    std::cout << "\nUniform Grid Result:\n";
    sim.PrintPairsResult(pairs2);
    std::chrono::duration<double, std::milli> elapsed2 = end2 - start2;
    std::cout << "Time Spend: " << elapsed2.count() << " ms\n";


    // case3: octree
    /*
    auto start3 = std::chrono::high_resolution_clock::now();
    auto pairs3 = sim.Octree();
    auto end3 = std::chrono::high_resolution_clock::now();
    std::cout << "OctreeResult:\n";
    sim.PrintPairsResult(pairs3);
    std::chrono::duration<double, std::milli> elapsed3 = end3 - start3;
    std::cout << "Time Spend: " << elapsed3.count() << " ms\n";
    */
    return 0;
}