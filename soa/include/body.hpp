#pragma once
#include <vector>
class Body {
public:
    float mass;
    float radius;
    std::vector<float> posx, posy, posz;
    std::vector<float> vecx, vecy, vecz;
    std::vector<float> accx, accy, accz;
    Body(): mass(0.0f), radius(0.0f), 
            posx(0.0f), posy(0.0f), posz(0.0f),
            vecx(0.0f), vecy(0.0f), vecz(0.0f),
            accx(0.0f), accy(0.0f), accz(0.0f){}

};