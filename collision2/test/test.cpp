#include "simulation.h"
#include "scenario.h"
#include "brute_force.h"
#include <vector>
#include <iostream>
int main() {
    // build particles scene-----------------------------------------------------
    std::vector<Particle> particles = scenario::two_particle_bounce_scenario();
    // --------------------------------------------------------------

    // set up configuration------------------------------------------
    SimulationConfig cfg;
    cfg.dt = 1.0f / 60.0f;
    cfg.K = 2.0f;
    cfg.method = BroadPhaseMethod::UniformGrid;
    cfg.cellSize = 1.0f;
    cfg.maxDepth = 8;
    cfg.leafCapacity = 8;
    cfg.worldSize = 100.0f;
    // --------------------------------------------------------------
    
    // build Simulation system---------------------------------------
    int totalFrames = 200;
    Simulation sim(particles, cfg, totalFrames);
    // --------------------------------------------------------------


    
}

