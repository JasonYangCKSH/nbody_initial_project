#pragma once
#include <glm/glm.hpp>
#include <vector>
class BodySystem {
public:

    std::vector<float> mass;
    std::vector<float> radius;


    std::vector<float> posX, posY, posZ;
    std::vector<float> velX, velY, velZ;
    std::vector<float> accX, accY, accZ;

    
    BodySystem() {}

    /**
     * core constructor
     */
    void addBody(glm::vec3 _pos, glm::vec3 _vel, glm::vec3 _acc, float _mass, float _radius) {
        posX.push_back(_pos.x); posY.push_back(_pos.y);posZ.push_back(_pos.z);

        velX.push_back(_vel.x); velY.push_back(_vel.y); velZ.push_back(_vel.z);

        accX.push_back(_acc.x); accY.push_back(_acc.y); accZ.push_back(_acc.z);

        mass.push_back(_mass);
        radius.push_back(_radius);
    }

    // unsigned long int 
    size_t size() const {
        return posX.size();
    }

    
    void reserve(size_t n) {
        posX.reserve(n); posY.reserve(n); posZ.reserve(n);
        velX.reserve(n); velY.reserve(n); velZ.reserve(n);
        accX.reserve(n); accY.reserve(n); accZ.reserve(n);
        mass.reserve(n); 
        radius.reserve(n);
    }
    void update(float dt) {
        for (int i = 0; i < posX.size(); i++) {
            vecX[i] = accX[i] * dt; vecY[i] = accY[i] * dt; vecZ[i] = accZ[i] * dt;
            posX[i] = vecX[i] * dt; posY[i] = vecY[i] * dt; posZ[i] = vecZ[i] * dt;
        }
    }
};