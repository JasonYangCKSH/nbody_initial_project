#include "TreeNSearch"
#include "point_set.h"
#include "simulation.h"
#include <vector>
#include <array>
#include <iostream>
#include <random>
int main() {
    Simulation sim;
    sim.SetDataSet(RANDOM, 100000);
    std::cout << "completed\n";
    return 0;
    
}