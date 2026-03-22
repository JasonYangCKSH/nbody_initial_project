#include <iostream>
#include <glm/glm.hpp>
#include "body.hpp"
#include "Barnes-HutOctree.hpp"
#include "simulation.hpp"
using namespace std;
int main() {
    // position range: ([-100, 100], [-100, 100], [-100, 100])
    // Simulation sim(0.01, 0.0, 0.1, NeighborMethod::BRUTE_FORCE, 0.1);
    float dt = 0.01f;
    float theta = 0.5f;
    float epsilon = 0.1f;
    NeighborMethod neighbor_method = NeighborMethod::BRUTE_FORCE;
    std::vector<Body> bodies;
    Simulation sim(dt, theta, epsilon, bodies, neighbor_method);
    return 0;
}