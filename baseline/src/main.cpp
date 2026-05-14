#include <iostream>
#include "simulation.hpp"
int main() {
    Simulation sim(1.0f); // searchRadius = 1.0 = 2 * body_radius, which is something to do with body's radius(0.5)
    sim.GenerateSimple();
    auto pairs = sim.BruteForce();
    sim.PrintPairsResult(pairs);
    return 0;
}