#include "simulation.h"
#include "scenario.h"
#include <vector>
#include <iostream>
int main() {


    float worldSize = 100.0f;

    // build particles scene-----------------------------------------------------
    std::vector<Particle> particles = scenario::uniformCloud(10000, worldSize, 1.0f, 1.0f );
    // --------------------------------------------------------------

    // set up configuration------------------------------------------
    SimulationConfig cfg;
    cfg.dt = 1.0f / 60.0f;
    cfg.K = 2.0f;
    cfg.method = Method::Octree;
    cfg.hasSkin = true;
    cfg.cellSize = 1.0f;
    cfg.maxDepth = 7;
    cfg.leafCapacity = 200;
    cfg.worldSize = worldSize;
    // --------------------------------------------------------------
    
    // build Simulation system---------------------------------------
    int totalFrames = 2;
    Simulation sim(particles, cfg, totalFrames);
    std::vector<FrameStats> frameStats;
    frameStats = sim.run();
    for (FrameStats& f: frameStats) {
        std::cout << f.frameIndex + 1 << " | " <<f.broadPhaseTimeMs << " | "<<f.narrowPhaseTimeMs<< "|" << 
                     f.candidateCount() <<" | " <<f.collisionCount()<< " | "<< f.didRebuild<<std::endl;
    }
    // --------------------------------------------------------------



}

