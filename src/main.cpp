#include <iostream>
#include <chrono>
#include <glm/glm.hpp>
#include "body.hpp"
#include "Barnes-HutOctree.hpp"
#include "simulation.hpp"
#include "senario.hpp"
auto now() {
    return std::chrono::high_resolution_clock::now();
}
double ms(double start, double end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}
int main() {
    // position range: ([-100, 100], [-100, 100], [-100, 100])
    
    int N = 10000;
    float range = 100;
    float mass = 1.0f;
    float radius = 1.0f;

    float dt = 0.01f;
    float theta = 0.5f;
    float epsilon = 0.1f;
    
    NeighborMethod neighbor_method = NeighborMethod::BRUTE_FORCE;
    std::vector<Body> bodies;

    bodies = Senario::UniformRandom(N, range, mass, radius);




    Simulation sim(dt, theta, epsilon, bodies, neighbor_method);
    return 0;
}