#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <utility>
using PairList = std::vector<std::pair<int, int>>;
struct Particle {
    glm::vec3 pos{0.0};
    glm::vec3 vel{0.0};
    glm::vec3 acc{0.0};
    float radius = 0.0f;


    glm::vec3 posAtLastBroadPhase{0.0f};
    float skin = 0.0f;

};