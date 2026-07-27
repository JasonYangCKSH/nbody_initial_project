#include "nbody/body.h"
#include "nbody/brute_force.h"
#include "nbody/integrator.h"
#include <vector>
#include <iostream>
#include <cmath>

double totalEnergy(const std::vector<Body>& bodies, double G) {
    double KE = 0.0, PE = 0.0;
    for (const auto& b : bodies) KE += 0.5 * b.mass * b.velocity.dot(b.velocity);
    for (size_t i = 0; i < bodies.size(); ++i)
        for (size_t j = i + 1; j < bodies.size(); ++j)
            PE -= G * bodies[i].mass * bodies[j].mass / (bodies[j].position - bodies[i].position).norm();
    return KE + PE;
}

int main() {
    const double G = 1.0;
    std::vector<Body> bodies(2);
    bodies[0].mass = 10.0;
    bodies[1].mass = 1.0;
    bodies[1].position = Vec3(1.0, 0.0, 0.0);

    double r = 1.0;
    double v_circ = std::sqrt(G * bodies[0].mass / r);
    bodies[1].velocity = Vec3(0.0, v_circ, 0.0);

    BruteForceCalculator calc(G, 0.0);
    LeapfrogIntegrator integrator(calc);

    calc.computeAccelerations(bodies);

    double E0 = totalEnergy(bodies, G);
    double theoreticalPeriod = 2 * M_PI * std::sqrt(std::pow(r, 3) / (G * (bodies[0].mass + bodies[1].mass)));

    double dt = theoreticalPeriod / 1000.0;
    for (int s = 0; s < 1000; ++s) {
        integrator.step(bodies, dt);
    }

    double E1 = totalEnergy(bodies, G);
    std::cout << "Energy relative error: " << std::abs((E1 - E0) / E0) << "\n";
    std::cout << "Theoretical period: " << theoreticalPeriod << "\n";
    std::cout << "Final position: (" << bodies[1].position.x << ", "
               << bodies[1].position.y << ", " << bodies[1].position.z << ")\n";
}