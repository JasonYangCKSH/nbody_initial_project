#ifndef BODY_H
#define BODY_H
#include <glm/glm.hpp>

class Body {
public:
    glm::vec3 pos;  // position 
    glm::vec3 vel;  // velocity
    glm::vec3 acc;  // acceleration
    float mass;
    float radius;
    Body() : pos(0.0f), vel(0.0f), acc(0.0f), mass(0.0f), radius(0.0f) {}
    Body(glm::vec3 _pos, glm::vec3 _vel, glm::vec3 _acc, float _mass, float _radius) {
        pos = _pos;
        vel = _vel;
        acc = _acc;
        mass = _mass;
        radius = _radius;
    }
    void Update(float dt) {
        vel += acc * dt; // v0 = v0 + a*t
        pos += vel * dt; // r0 = r0 + v*t
    }

};
#endif