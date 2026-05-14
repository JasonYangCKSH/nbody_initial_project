#include "simulation.hpp"
#include <cmath>

std::vector<NeighborPair> Simulation::BruteForce() {
    if (bodies.empty()) return {};
    
    std::vector<NeighborPair> pairs;
    float h2 = searchRadius * searchRadius;

    for (int i = 0; i < (int)bodies.size(); i++) {
        for (int j = i + 1; j < (int)bodies.size(); j++) {
            glm::vec3 d = bodies[j].pos - bodies[i].pos;
            float dist2 = d.x*d.x + d.y*d.y + d.z*d.z;
            if (dist2 > h2) continue;
            float dist = std::sqrt(dist2);
            pairs.push_back({i, j, dist});
        }
    }
    return pairs;
}