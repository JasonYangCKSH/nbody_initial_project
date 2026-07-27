#pragma once
#include "body.h"
#include <vector>
class ForceCalculator {
public:
    virtual void computeAccelerations(std::vector<Body> &bodies) = 0;
    virtual ~ForceCalculator() = default;
};