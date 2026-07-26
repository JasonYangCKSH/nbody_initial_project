#pragma once
#include "vec3.h"
class Body {
private:
    int id;
public:
    
    double mass;
    double radius;
    
    Vec3 position;
    Vec3 velocity;
    Vec3 acceleration;

    Body(){}
    Body(int _id, double _mass, double _radius, Vec3 _position, Vec3 _velocity, Vec3 _acceleration):
    id(_id), mass(_mass), radius(_radius), position(_position), velocity(_velocity), acceleration(_acceleration){}

    int getID() const {return this->id;};
};