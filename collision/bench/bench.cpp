#include "../include/particle.h"
#include "../include/scenarios.h"
#include "../include/simulation.h"
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

int main() {
    // 1.
    float radius = 1.0f;
    float cellSize = 2.0f;
    float speed = 100.0f;
    // 2.
    scenario::Cloud particles = scenario::uniformCloud(10000, 50.0f, radius, speed);
    scenario::Cloud refParticles = particles; // 給 brute force 用的獨立副本
    // 3.
    SimConfig config;
    config.cellSize = cellSize;
    config.K = 0;
    config.skinMode = SimConfig::SkinMode::LocalVelocity;

    Simulation sim(config);


    for (int i = 0; i < 2000; i++) {
        StepStats st = sim.step(particles);
        std::cout << "step " << i + 1
                << " | rebuilt: " << (st.broadPhaseExecuted ? "yes" : "no")
                << " | pairs: " << st.broadPhasePairs
                << " | collisions: " << st.collisionCount
                << " | broadPhase: " << st.broadPhaseSeconds << "s"
                << " | narrowPhase: " << st.narrowPhaseSeconds << "s"
                << "\n";
    }


}