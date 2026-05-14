#include "simulation.hpp"
#include <cmath>
#include <iostream>
#include <iomanip>
// A
void Simulation::GenerateSimple() {
    bodies.clear();
    // pos|vel|acc|mass|radius
    bodies.push_back(Body(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0), glm::vec3(0), 1.0f, 0.5f));
    bodies.push_back(Body(glm::vec3(0.8f, 0.0f, 0.0f), glm::vec3(0), glm::vec3(0), 1.0f, 0.5f));
    bodies.push_back(Body(glm::vec3(5.0f, 0.0f, 0.0f), glm::vec3(0), glm::vec3(0), 1.0f, 0.5f));
    bodies.push_back(Body(glm::vec3(5.8f, 0.0f, 0.0f), glm::vec3(0), glm::vec3(0), 1.0f, 0.5f));
}
// B
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

// C

void Simulation::PrintPairsResult(const std::vector<NeighborPair>& pairs) const {
    std::cout << std::left 
            << std::setw(15) << "INDEX_I" 
            << std::setw(15) << "INDEX_J" 
            << "DISTANCE" << "\n";
    std::cout << std::string(45, '-') << "\n";

    for (const auto& pair : pairs) {
        std::cout << std::left 
                << "INDEX: [" << std::setw(4) << pair.i << "], "
                << "[" << std::setw(4) << pair.j << "]; "
                << "DISTANCE: {" << std::fixed << std::setprecision(4) << pair.distance << "};\n";
    }
}