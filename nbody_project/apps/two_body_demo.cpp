#include "nbody/body.h"
#include "nbody/barnes_hut.h"
#include "nbody/brute_force.h"
#include "nbody/integrator.h"
#include "nbody/diagnostics.h"
#include <vector>
#include <iostream>
#include <fstream>
#include <cmath>

int main() {
    const double G = 1.0;

    std::vector<Body> bodies(2);
    bodies[0].mass = 2.0;
    bodies[1].mass = 1.0;
    bodies[1].position = Vec3(1.0, 0.0, 0.0);

    double r = 1.0;
    double v_circ = std::sqrt(G * (bodies[0].mass + bodies[1].mass) / r);
    bodies[1].velocity = Vec3(0.0, v_circ, 0.0);

    BarnesHutCalculator calc(G, 0.0, 0.0);
    LeapfrogIntegrator integrator(calc);

    calc.computeAccelerations(bodies);

    // CSV輸出設定
    std::ofstream outFile("../data/output/trajectory.csv");
    outFile << "step,body0_x,body0_y,body1_x,body1_y\n";
    outFile << 0 << "," << bodies[0].position.x << "," << bodies[0].position.y
             << "," << bodies[1].position.x << "," << bodies[1].position.y << "\n";

    // 模擬前的診斷值
    double E0 = totalEnergy(bodies, G);
    Vec3 L0 = totalAngularMomentum(bodies);

    double theoreticalPeriod = 2 * M_PI * std::sqrt(std::pow(r, 3) / (G * (bodies[0].mass + bodies[1].mass)));
    double dt = theoreticalPeriod / 10000.0;

    for (int s = 0; s < 10000; ++s) {
        integrator.step(bodies, dt);
        outFile << (s + 1) << "," << bodies[0].position.x << "," << bodies[0].position.y
                 << "," << bodies[1].position.x << "," << bodies[1].position.y << "\n";
    }

    outFile.close();

    // 模擬後的診斷值
    double E1 = totalEnergy(bodies, G);
    Vec3 L1 = totalAngularMomentum(bodies);

    double energyError = std::abs((E1 - E0) / E0);
    double angMomError = (L1 - L0).norm() / L0.norm();

    double separation = (bodies[1].position - bodies[0].position).norm();

    std::cout << "mass0: [" << bodies[0].mass << "] | mass1: [" << bodies[1].mass << "]\n";
    std::cout << "Energy relative error: " << energyError << "\n";
    std::cout << "Angular momentum relative error: " << angMomError << "\n";
    std::cout << "Theoretical period: " << theoreticalPeriod << "\n";
    std::cout << "bodies[0] Final position: (" << bodies[0].position.x << ", "
               << bodies[0].position.y << ", " << bodies[0].position.z << ")\n";
    std::cout << "bodies[1] Final position: (" << bodies[1].position.x << ", "
               << bodies[1].position.y << ", " << bodies[1].position.z << ")\n";
    std::cout << "Final separation: " << separation << " (should be close to " << r << ")\n";

    return 0;
}