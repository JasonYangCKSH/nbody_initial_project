#pragma once
#include "force_calculator.h"

class BruteForceCalculator : public ForceCalculator {
public:

    BruteForceCalculator(double _G = 1.0, double _softening = 0.0): G(_G), softening(_softening){}

    void computeAccelerations(std::vector<Body>& bodies) override;

private:

    double G;
    double softening;
};