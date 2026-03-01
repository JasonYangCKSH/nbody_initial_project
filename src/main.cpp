#include <iostream>
#include <vector>
#include <fstream>
#include <random>
#include <cmath>
#include <chrono>
#include <glm/glm.hpp>
#include "simulation.h"
#include "body.h"

int main() {

    float dt = 0.01f;
    float theta = 0.5f;
    float epsilon = 0.1f;

    Simulation sim(dt, theta, epsilon);

    std::mt19937 gen(42);
    std::uniform_real_distribution<float> disPos(-50.0f, 50.0f);
    size_t bodyNum = 10000;
    for (size_t i = 0; i < bodyNum; i++) {

        Body b;
        b.pos = glm::vec3(disPos(gen), disPos(gen), disPos(gen));
        b.vel = glm::vec3(0.0f); 
        b.mass = 5.0f;
        b.radius = 0.2f;
        sim.bodies.push_back(b);
    }

    int totalStep = 100;
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < totalStep; i++) {
        //std::cout << "Processing Step: " << i + 1 << std::endl;
        sim.step();
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> time = end - start;
    std::cout << "Time Spend: " << time.count()<< std::endl;

    return 0;
}