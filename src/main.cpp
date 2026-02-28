#include <iostream>
#include <vector>
#include <glm/glm.hpp>
#include "simulation.h"
#include "body.h"

int main() {
    // 1. Initialize Simulation Parameters
    // dt = 0.01s, theta = 0.5 (accuracy), epsilon = 0.1 (softening)
    float dt = 0.01f;
    float theta = 0.5f;
    float epsilon = 0.1f;
    Simulation sim(dt, theta, epsilon);

    // 2. Create some initial bodies
    // Example: Two bodies attracting each other
    Body sun(
        glm::vec3(0.0f, 0.0f, 0.0f),    // Position
        glm::vec3(0.0f, 0.0f, 0.0f),    // Velocity
        glm::vec3(0.0f, 0.0f, 0.0f),    // Acceleration
        1000.0f,                        // Mass
        1.0f                            // Radius
    );

    Body planet(
        glm::vec3(10.0f, 0.0f, 0.0f),   // Position
        glm::vec3(0.0f, 5.0f, 0.0f),    // Velocity (Orbiting speed)
        glm::vec3(0.0f, 0.0f, 0.0f),    // Acceleration
        1.0f,                           // Mass
        0.2f                            // Radius
    );

    sim.bodies.push_back(sun);
    sim.bodies.push_back(planet);

    std::cout << "Starting Simulation with " << sim.bodies.size() << " bodies..." << std::endl;
    std::cout << "----------------------------------------------" << std::endl;

    // 3. Run Simulation Loop
    int total_steps = 100;
    for (int i = 0; i < total_steps; ++i) {
        sim.step();

        // Print progress every 10 steps
        if (i % 10 == 0) {
            std::cout << "Frame: " << sim.frame << std::endl;
            for (size_t b = 0; b < sim.bodies.size(); ++b) {
                const auto& p = sim.bodies[b].pos;
                std::cout << "  Body " << b << " Pos: (" << p.x << ", " << p.y << ", " << p.z << ")" << std::endl;
            }
            std::cout << "-----------------------" << std::endl;
        }
    }

    std::cout << "Simulation finished." << std::endl;

    return 0;
}