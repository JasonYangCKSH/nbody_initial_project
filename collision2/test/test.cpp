#include <iostream>
#include <vector>
#include <algorithm>
#include <random>
#include <cmath>
#include <chrono>
#include "particle.h"
#include "broad_phase.h"
#include "brute_force.h"

enum class ScenarioType {
    UnitTest_TwoOverLapping,
    UnitTest_BarelyTouching,
    UnitTest_NoTouching,
    UniformRandom,
    DenseCluster
};
std::vector<Particle> generateScenario(ScenarioType type, size_t macroCount, float worldSize) {
    std::vector<Particle> particles;

    switch (type) {
        // --- 微觀單元測試 ---
        case ScenarioType::UnitTest_TwoOverLapping: {
            Particle p1, p2;
            p1.pos = glm::vec3(0.0f, 0.0f, 0.0f); p1.radius = 0.5f; p1.skin = 0.05f;
            p2.pos = glm::vec3(0.5f, 0.0f, 0.0f); p2.radius = 0.5f; p2.skin = 0.05f;
            particles = {p1, p2};
            break;
        }
        case ScenarioType::UnitTest_BarelyTouching: {
            Particle p1, p2;
            p1.pos = glm::vec3(0.0f, 0.0f, 0.0f); p1.radius = 0.5f; p1.skin = 0.0f;
            p2.pos = glm::vec3(1.0f, 0.0f, 0.0f); p2.radius = 0.5f; p2.skin = 0.0f; // 距離剛好等於 r1 + r2
            particles = {p1, p2};
            break;
        }

        case ScenarioType::UnitTest_NoTouching: {
            Particle p1, p2;
            p1.pos = glm::vec3(0.0f, 0.0f, 0.0f); p1.radius = 0.5f; p1.skin = 0.0f;
            p2.pos = glm::vec3(2.0f, 0.0f, 0.0f); p2.radius = 0.5f; p2.skin = 0.0f;
            particles = {p1, p2};
            break;
        }
        // --- 宏觀極端測試 ---
        case ScenarioType::UniformRandom: {
            particles.reserve(macroCount);
            std::mt19937 rng(42); 
            float halfSize = worldSize * 0.45f; 
            std::uniform_real_distribution<float> posDist(-halfSize, halfSize);
            std::uniform_real_distribution<float> radiusDist(0.1f, 0.5f);

            for (size_t i = 0; i < macroCount; ++i) {
                Particle p;
                p.pos = glm::vec3(posDist(rng), posDist(rng), posDist(rng));
                p.radius = radiusDist(rng);
                p.skin = 0.02f;
                particles.push_back(p);
            }
            break;
        }
        case ScenarioType::DenseCluster: {
            particles.reserve(macroCount);
            std::mt19937 rng(42);
            // 使用高斯分佈 (標準差設為世界大小的 2%)，強行將 99% 粒子塞在中心極小區域
            std::normal_distribution<float> posDist(0.0f, worldSize * 0.02f);
            std::uniform_real_distribution<float> radiusDist(0.1f, 0.5f);

            for (size_t i = 0; i < macroCount; ++i) {
                Particle p;
                p.pos = glm::vec3(posDist(rng), posDist(rng), posDist(rng));
                p.radius = radiusDist(rng);
                p.skin = 0.02f;
                particles.push_back(p);
            }
            break;        
        }
        default: {
            break;
        }
    }
    return particles;
}

int main() {
    std::vector<Particle> particles = generateScenario(ScenarioType::DenseCluster, (size_t)1500, 100.0f);
    // 1. BruteForce
    auto start1 = std::chrono::high_resolution_clock::now();
    PairList pairs1 = BruteForce(particles);
    auto end1 = std::chrono::high_resolution_clock::now();

    // 2. Uniform Grid
    broad::UniformGrid uni(0.8);
    auto start2 = std::chrono::high_resolution_clock::now();
    PairList pairs2 = uni.Build(particles, false);
    auto end2 = std::chrono::high_resolution_clock::now();

    // 3. Linear Octree
    broad::Octree oct(1, 1, 100.0f);
    auto start3 = std::chrono::high_resolution_clock::now();
    PairList pairs3 = oct.Build(particles, false);
    auto end3 = std::chrono::high_resolution_clock::now();

    std::cout << pairs1.size() << " " << pairs2.size() << " " << pairs3.size();

    //std::cout << "BruteForce: " 
    //          << std::chrono::duration<double, std::milli>(end1 - start1).count() << " ms\n";
    //std::cout << "UniformGrid: " 
    //          << std::chrono::duration<double, std::milli>(end2 - start2).count() << " ms\n";
    //std::cout << "Octree: " 
    //          << std::chrono::duration<double, std::milli>(end3 - start3).count() << " ms\n";
    /*for (auto& p1: pairs1) {
        std::cout << "[Brute Force]: " << p1.first << ", " << p1.second << std::endl;
    }
    for (auto& p2: pairs2) {
        std::cout << "[Uniform Grid]: " << p2.first << ", " << p2.second << std::endl;
    }
    for (auto& p3: pairs3) {
        std::cout << "[Octree]: " << p3.first << ", " << p3.second << std::endl;

    }*/
}