#include <iostream>
#include "simulation.hpp"
int main() {
    Simulation sim(1.0f); // searchRadius = 1.0
    sim.GenerateSimple();
    auto pairs = sim.BruteForce();
    sim.PrintPairsResult(pairs);
    return 0;
}