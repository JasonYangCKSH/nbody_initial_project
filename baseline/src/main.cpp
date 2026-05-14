#include <iostream>
#include "simulation.hpp"
int main() {
    Simulation sim(0.8001f); // searchRadius = 1.0
    sim.GenerateSimple();
    auto pairs = sim.BruteForce();
    sim.PrintPairsResult(pairs);
    return 0;
}