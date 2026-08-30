#include "simulation.h"
#include "scenario.h"
#include <vector>
#include <iostream>
int main() {




    // build particles scene-----------------------------------------------------
    std::vector<Particle> particles = scenario::uniformCloud(100000, 100.0f, 1.0f, 1.0f );
    // --------------------------------------------------------------

    // set up configuration------------------------------------------
    SimulationConfig cfg;
    cfg.dt = 1.0f / 60.0f;
    cfg.K = 2.0f;
    cfg.method = Method::UniformGrid;
    cfg.hasSkin = false;
    cfg.cellSize = 1.0f;
    cfg.maxDepth = 7;
    cfg.leafCapacity = 30;
    cfg.worldSize = 100.0f;
    // --------------------------------------------------------------
    
    // build Simulation system---------------------------------------
    int totalFrames = 6;
    Simulation sim(particles, cfg, totalFrames);
    std::vector<FrameStats> frameStats;
    frameStats = sim.run();
    for (FrameStats& f: frameStats) {
        std::cout << f.frameIndex + 1 << " | " <<f.broadPhaseTimeMs << " | "<<f.narrowPhaseTimeMs<< "|" << 
                     f.candidateCount() <<" | " <<f.collisionCount()<< " | "<< f.didRebuild<<std::endl;
    }
    // --------------------------------------------------------------



}

