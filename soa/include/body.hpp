#pragma once
#include <glm/glm.hpp>
#include <vector>
class Body {
public:
    float mass;
    float radius;
    std::vector<float> posX, posY, posZ;
    std::vector<float> vecX, vecY, vecZ;
    std::vector<float> accX, accY, accZ;
    Body(): mass(0.0f), radius(0.0f), 
            posX(0.0f), posY(0.0f), posZ(0.0f),
            vecX(0.0f), vecY(0.0f), vecZ(0.0f),
            accX(0.0f), accY(0.0f), accZ(0.0f){}
    Body(glm::vec3 _pos, glm::vec3 _vel, glm::vec3 _acc, float _mass, float _radius) {

    }
};