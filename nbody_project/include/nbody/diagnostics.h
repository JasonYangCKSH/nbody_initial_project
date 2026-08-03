#pragma once
#include "nbody/body.h"
#include <vector>

// totalEnergy
double totalEnergy(const std::vector<Body>& bodies, double G) {
    // KE:動能 PE:位能
    double KE = 0.0, PE = 0.0;
    for (const auto& b : bodies) KE += 0.5 * b.mass * b.velocity.dot(b.velocity);
    for (size_t i = 0; i < bodies.size(); ++i)
        for (size_t j = i + 1; j < bodies.size(); ++j)
            PE -= G * bodies[i].mass * bodies[j].mass / (bodies[j].position - bodies[i].position).norm();
    return KE + PE;
}
// totalAngularMomentum
inline Vec3 totalAngularMomentum(const std::vector<Body>& bodies) {
    Vec3 L(0.0, 0.0, 0.0);
    for (const auto& b : bodies) {
        L += b.position.cross(b.velocity) * b.mass;
    }
    return L;
}