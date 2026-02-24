#ifndef SIMULATION_H
#define SIMULATION_H
#include <vector>
#include "octree.h"
#include "body.h"
class Simulation {
public:
    float dt;
    int frame;
    std::vector<Body> bodies;
    Octree octree;
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