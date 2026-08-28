#pragma once
#include "particle.h"
#include "broad_phase.h"
#include "narrow_phase.h"
#include "verlet_buffer.h"
#include "scenario.h"
#include <vector>
#include <chrono>
#include <iostream>
#include <glm/glm.hpp>


class Simulation {
private:
    int totalTimeFrame_;
    int currentTimeFrame_;
    Scenario scenario_;
    
public:

};