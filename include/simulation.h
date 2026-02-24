#ifndef SIMULATION_H
#define SIMULATION_H
#include <vector>
#include <glm/glm.hpp>
#include "octree.h"
#include "body.h"
class Simulation {
public:
    float dt;  // time step
    int frame;  // current frame number
    std::vector<Body> bodies;  // all bodies in the simulation
    Octree octree;  // Barnes-Hut octree for efficient force calculation
    Simulation(float _dt, float theta, float epsilon) :
        dt(_dt), frame(0), octree(theta, epsilon) {}
    void step() {

    }

private:
    void iterate() {

    }
    void collide() {

    }
    void attract() {

    }
};
#endif