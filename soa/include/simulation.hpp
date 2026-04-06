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
private:
};