#include "include/simulation.hpp"


std::vector<NeighborPair> Simulation::BruteForce() {

    if (bodies.empty()) return {};
    std::vector<NeighborPair> pairs;
    for (int i = 0; i < (int)bodies.size(); i++) {
        for (int j = i + 1; j < (int)bodies.size(); j++) {
            float combinedRadius = bodies[i].radius + bodies[j].radius;
            glm::vec3 d = bodies[j].pos - bodies[i].pos;
            float dist2 = d.x*d.x + d.y*d.y + d.z*d.z;
            if (dist2 > combinedRadius * combinedRadius) continue;
            pairs.push_back({i, j});
        }
    }
    return pairs;

}