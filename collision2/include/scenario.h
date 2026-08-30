#pragma once

#include "particle.h"
#include <vector>

namespace scenario {

inline std::vector<Particle> two_particle_bounce_scenario() {
    std::vector<Particle> particles(2);

    particles[0].pos = glm::vec3(-0.60f, 0.0f, 0.0f);
    particles[0].vel = glm::vec3(1.0f, 0.0f, 0.0f);
    particles[0].acc = glm::vec3(0.0f, 0.0f, 0.0f);
    particles[0].radius = 0.5f;
    particles[0].skin = 0.05f;

    particles[1].pos = glm::vec3(0.60f, 0.0f, 0.0f);
    particles[1].vel = glm::vec3(-1.0f, 0.0f, 0.0f);
    particles[1].acc = glm::vec3(0.0f, 0.0f, 0.0f);
    particles[1].radius = 0.5f;
    particles[1].skin = 0.05f;

    return particles;
}

}
