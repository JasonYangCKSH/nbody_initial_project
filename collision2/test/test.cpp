#include <iostream>
#include <vector>
#include <algorithm>
#include <random>
#include <cmath>
#include "particle.h"
#include "broad_phase.h"
#include "brute_force.h"

enum class ScenarioType {
    UnitTest_TwoOverLapping,
    UnitTest_BarelyTouching,
    UnitTest_NoTouching
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
        default: {
            break;
        }
    }
    return particles;
}

int main() {
    std::vector<Particle> particles = generateScenario(ScenarioType::UnitTest_NoTouching, (size_t)10000, 25.0f);
    PairList pairs1 = BruteForce(particles, false);
    broad::UniformGrid uni(0.8);
    PairList pairs2 = uni.Build(particles, false);

    for (auto& p1: pairs1) {
        std::cout << "[Brute Force]: " << p1.first << ", " << p1.second << std::endl;
    }
    for (auto& p2: pairs2) {
        std::cout << "[Uniform Grid]: " << p2.first << ", " << p2.second << std::endl;
    }
}