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
    // Kinematics "Update"
    void iterate() {
        for (Body& body : bodies) 
            body.update(dt);
    }
    // Barnes-Hut Logic
    void attract() {
        // 1.set up octree boundary
        Oct boundary = Oct().new_containing(bodies);

        // 2.clear and rebuild octree ==> BOTTLENECK
        octree.clear(boundary);
        for (Body& body : bodies)
            octree.insert(body.pos, body.mass);

        // 3.propagate mass and center of mass up the tree
        octree.propagate();

        // 4.calculate acceleration for each body
        for (Body& body : bodies)
            body.acc = octree.calculate_acc(body.pos);
        
    }
    // Broad-phase collision detection and narrow-phase resolutionsss
    void collide() {

    }
    // Resolve collision between body i and body j
    void resolve(int i, int j) {

    }
};
#endif