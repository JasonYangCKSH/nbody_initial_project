#include "simulation.h"
#include "scenario.h"
#include <vector>
#include <iostream>
int main() {


    float worldSize = 100.0f;

    // build particles scene-----------------------------------------------------
    //auto particles = scenario::two_particle_bounce_scenario();
    std::vector<Particle> particles = scenario::explosion(10000, worldSize, 1.0f, 1.0f );
    // --------------------------------------------------------------

    // set up configuration------------------------------------------
    SimulationConfig cfg(
        1.0f / 60.0f,       // dt
        2.0f,                // K
        true,                // hasSkin
        Method::Octree,      // method
        1.0f,                // cellSize
        7,                   // maxDepth
        2,                 // leafCapacity
        worldSize            // worldSize
    );
    // --------------------------------------------------------------
    
    // build Simulation system---------------------------------------
    int totalFrames = 5;
    Simulation sim(particles, cfg, totalFrames);
    std::vector<FrameStats> frameStats;
    frameStats = sim.run();
    for (FrameStats& f: frameStats) {
        std::cout << f.frameIndex + 1 << " | " <<f.broadPhaseTimeMs << " | "<<f.narrowPhaseTimeMs<< "|" << 
                     f.candidateCount() <<" | " <<f.collisionCount()<< " | "<< f.didRebuild<<std::endl;
    }
    // --------------------------------------------------------------



}

