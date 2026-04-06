#pragma once 
#include <vector>
#include <iostream>
#include <glm/glm.hpp>
#include <utility>
#include "body_system.hpp"
#include "Barnes-HutOctree.hpp"
class Simulation {
public:
    float dt;
    int frame;
    BodySystem bs;

    // Barnes-Hut 
    Octree octree;
    Oct boundary;

    Simulation(float _dt, float theta, float epsilon, const BodySystem& _bs): dt(_dt), frame(0), 
    bs(_bs), octree(theta, epsilon), boundary(){}
    void step() {
        this->iterate();
        this->collide();
        this->attract();
        frame++;
    }
private:
    void iterate() {
        bs.update(dt);
    }
    void attract() {
        boundary = Oct().new_containing();
        octree.clear(boundary);
        for (int i = 0; i < bs.posX.size(); i++) {
            octree.insert(bs, i);
        }
        octree.propagate();
        for (int i = 0; i < bs.posX.size(); i++) {
            glm::vec3 acc = octree.calculate_acc(bs.posX[i], bs.posY[i], bs.posZ[i]);
            bs.accX[i] = acc.x;
            bs.accY[i] = acc.y;
            bs.accZ[i] = acc.z;
        }        

    }
    void collide() {

    }

    void resolve() {

    }
};