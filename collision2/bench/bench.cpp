#include <iostream>
#include <vector>
#include "scenario.h"
#include "broad_phase.h"
#include "narrow_phase.h"
#include "particle.h"
#include "brute_force.h"
#include "verlet_buffer.h"
#include "collision_response.h"
int main() {
    const int n = 10000;
    const float boxSize = 100.0f;
    const float radius = 1.0f;
    const float speed = 5.0f;
    const float acc = 10.0f;

    std::vector<Particle> particles = scenario::uniformCloud(n, boxSize, radius, speed, acc);
}