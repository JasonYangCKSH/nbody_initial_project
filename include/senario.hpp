#pragma once
#include <vector>
#include <random>
#include "body.hpp"
namespace Senario {
    // 1. uniform distributed bodies
    std::vector<Body> UniformRandom(int N, float range, float mass, float radius) {
        std::vector<Body> bodies;
        body.reserve(N);  // 預留N筆空間

        std::mt19937 rng(42);
        std::uniform_real_distribution<float> dist(-range, range);
    
        for (int i = 0; i < N; i++) {
            glm::vec3 pos = {dist(rng), dist(rng), dist(rng)};
            glm::vec3 vel = {0, 0, 0};
            glm::vec3 acc = {0, 0, 0};
            bodies.push_back(Body(pos, vel, acc, mass, radius));
        }
        return bodies;
    }
};