#pragma once
#include <vector>
#include <random>
#include "body.hpp"
namespace Senario {
    // 1. uniform distributed bodies
    std::vector<Body> UniformRandom(int N, float range, float mass, float radius) {
        std::vector<Body> bodies;
        body.reserve(N);  // reserve N bodies' space, enhance efficiency

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


    // 2. clustered
    std::vector<Body> clustered(int N, int numOfClusters, float clusterRadius, float mass, float radius, unsigned int seed = 42) {

    }
};