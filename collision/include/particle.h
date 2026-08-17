#pragma once
#include <glm/glm.hpp>

// A single DEM particle. Integration and force models live outside this
// struct (see simulation.h) so the collision-detection code below can stay
// independent of the physics model.
struct Particle {
    glm::vec3 pos{0.0f};
    glm::vec3 vel{0.0f};
    glm::vec3 acc{0.0f};
    float radius = 0.0f;

    // Local Verlet buffer bookkeeping (Checkaraou et al. 2022, section 4).
    glm::vec3 posAtLastBroadPhase{0.0f};
    float skin = 0.0f;
};
