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
        this->iterate();  // update positions and velocities
        this->collide(); // handle collisions
        this->attract(); // calculate gravitational forces and update accelerations
        frame++;
    }

private:
    void iterate() {
        for (Body& body : bodies) 
            body.update(dt);
    }
    void collide() {

    }
    void attract() {

    }
};
#endif