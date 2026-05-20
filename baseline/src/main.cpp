#include <iostream>
#include <chrono>
#include <fstream>
#include <vector>
#include "simulation.hpp"

int main() {
    
    
    Simulation sim(1.0f); // searchRadius = 1.0 = 2 * body_radius, which is something to do with body's radius(0.5)
    
    std::vector<int> bodyNumVec = {2000,  4000, 6000,  8000, 10000, 12000, 14000/*, 16000*/};
    
    
    std::ofstream of1("../graph/bruteforce.csv");
    std::ofstream of2("../graph/uniformgrid.csv");

    for (const int& bodyNum: bodyNumVec) {
        sim.GenerateRandom(bodyNum, -15.0f, 15.0f);
        // case1: brute force
        auto start = std::chrono::high_resolution_clock::now();
        std::cout << "\nBrute Force Processing...\n";
        auto pairs = sim.BruteForce();
        auto end = std::chrono::high_resolution_clock::now();
        
        // case2: uniform grid
        auto start2 = std::chrono::high_resolution_clock::now();
        std::cout << "Uniform Grid Processing...\n";
        auto pairs2 = sim.UniformGrid();
        auto end2 = std::chrono::high_resolution_clock::now();
        assert(pairs.size() == pairs2.size());


        //--------------Print out result-----------------------------
        std::cout << "\nBrute Force Result:\n";
        //sim.PrintPairsResult(pairs);
        std::chrono::duration<double, std::milli> elapsed = end - start;
        std::cout << "Time Spend: " << elapsed.count() << " ms\n";
        of1 << bodyNum << " " << elapsed.count() << "\n";


        std::cout << "\nUniform Grid Result:\n";
        //sim.PrintPairsResult(pairs2);
        std::chrono::duration<double, std::milli> elapsed2 = end2 - start2;
        std::cout << "Time Spend: " << elapsed2.count() << " ms\n";
        of2 << bodyNum << " " << elapsed2.count() << "\n";
        //------------------------------------------------------------

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
    }
    of1.close();
    of2.close();
    return 0;
}