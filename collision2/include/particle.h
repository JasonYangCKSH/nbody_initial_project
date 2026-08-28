#pragma once
#include <glm/glm.hpp>
struct Particle {
    glm::vec3 pos{0.0};
    glm::vec3 vec{0.0};
    glm::vec3 acc{0.0};
    float radius = 0.0f;


    glm::vec3 posAtLastBroadPhase{0.0f};
    float skin = 0.0f;

};