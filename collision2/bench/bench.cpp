#include "simulation.h"
#include "scenario.h"
#include <iostream>

int main() {
    auto particles = scenario::two_particle_bounce_scenario();

    SimConfig cfg;
    cfg.dt = 1.0f / 60.0f;
    cfg.cellSize = 100.0f;
    auto structure = std::make_unique<std::variant<broad::UniformGrid, broad::Octree>>(
        broad::Octree(10, 1, 100.0f));
    Simulation sim(100, StructureMode::Octree, std::move(structure), false, cfg);
    sim.InitializeParticles(particles);

    const int totalFrames = 1000;
    auto history = sim.runForFrames(totalFrames);

    std::cout << "frame,collision_count,broad_phase_pairs,total_collision_count\n";

    int totalCollisionCount = 0;
    int totalBroadPhasePairs = 0;

    for (int frame = 0; frame < (int)history.size(); ++frame) {
        const auto& stats = history[frame];
        totalCollisionCount += stats.collisionCount;
        totalBroadPhasePairs += stats.broadPhasePairs;

        std::cout << frame
                  << "," << stats.collisionCount
                  << "," << stats.broadPhasePairs
                  << "," << totalCollisionCount << "\n";
    }

    std::cout << "\nsummary\n";
    std::cout << "total_frames," << totalFrames << "\n";
    std::cout << "total_collision_count," << totalCollisionCount << "\n";
    std::cout << "total_broad_phase_pairs," << totalBroadPhasePairs << "\n";

    return 0;
}