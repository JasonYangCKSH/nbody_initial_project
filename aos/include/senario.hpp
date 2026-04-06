#pragma once
#include <vector>
#include <random>
#include "body.hpp"
namespace Senario {
    // 1. uniform distributed bodies
    std::vector<Body> UniformRandom(int N, float range, float mass, float radius) {
        std::vector<Body> bodies;
        bodies.reserve(N);  // reserve N bodies' space, enhance efficiency

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
    std::vector<Body> Clustered(int N, int numOfClusters, float worldRange,
                                float clusterRadius, float mass, float radius,
                                unsigned int seed = 42) {
        std::vector<Body> bodies;
        bodies.reserve(N);

        std::mt19937 rng(seed);
        std::uniform_real_distribution<float> worldDist(-worldRange, worldRange);
        std::normal_distribution<float> localDist(0.0f, clusterRadius);

        // Step 1：隨機產生 cluster 中心
        std::vector<glm::vec3> centers(numOfClusters);
        for (auto& c : centers)
            c = {worldDist(rng), worldDist(rng), worldDist(rng)};

        // Step 2：每個粒子隨機歸屬某個 cluster，圍繞中心做高斯分布
        std::uniform_int_distribution<int> clusterPick(0, numOfClusters - 1);
        for (int i = 0; i < N; i++) {
            glm::vec3 center = centers[clusterPick(rng)];
            glm::vec3 pos = center + glm::vec3(localDist(rng), 
                                            localDist(rng), 
                                            localDist(rng));
            bodies.push_back(Body(pos, {0,0,0}, {0,0,0}, mass, radius));
        }
        return bodies;
    }
    // 3. extreme cluster
    std::vector<Body> ExtremeClustered(int N, float mass, float radius,
                                    unsigned int seed = 42) {
        std::vector<Body> bodies;
        bodies.reserve(N);

        std::mt19937 rng(seed);
        // 極小的 clusterRadius：幾乎所有粒子擠在同一點
        std::normal_distribution<float> localDist(0.0f, 0.01f);

        // 只有 2 個 cluster，距離極近
        std::vector<glm::vec3> centers = {
            {0.0f,  0.1f, 0.0f},
            {0.0f, -0.1f, 0.0f}
        };

        std::uniform_int_distribution<int> clusterPick(0, 1);
        for (int i = 0; i < N; i++) {
            glm::vec3 center = centers[clusterPick(rng)];
            glm::vec3 pos = center + glm::vec3(localDist(rng),
                                            localDist(rng),
                                            localDist(rng));
            bodies.push_back(Body(pos, {0,0,0}, {0,0,0}, mass, radius));
        }
        return bodies;
    }
    std::vector<Body> NormalTestBench() {
        std::vector<Body> bodies;
        //bodies.reserve(5);
        //1
        bodies.push_back(Body(glm::vec3(-1.0f, 0, 0), glm::vec3( 0.5f, 0, 0), glm::vec3(0), 1.0f, 1.0f));
        bodies.push_back(Body(glm::vec3( 1.0f, 0, 0), glm::vec3(-0.5f, 0, 0), glm::vec3(0), 1.0f, 1.0f));
        return bodies;
    }
};