#include "TreeNSearch"
#include "point_set.h"
#include "simulation.h"
#include <vector>
#include <array>
#include <iostream>
#include <random>
int main() {
    Simulation sim;
    sim.SetDataSet(RANDOM, 10000);
    std::cout << sim.RunBruteForce() << " ms\n";
    std::cout << "completed\n";
    return 0;
    
}