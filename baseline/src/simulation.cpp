#include "simulation.hpp"
#include <cmath>
#include <iostream>
#include <iomanip>
#include <glm/glm.hpp>
#include <vector>
#include <array>
#include <random>
// A
void Simulation::GenerateSimple() {
    bodies.clear();
    // pos                              vel         acc         mass  radius
    // --- 群組 A：互相都是鄰居 ---
    bodies.push_back(Body(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0), glm::vec3(0), 1.0f, 0.5f)); // 0
    bodies.push_back(Body(glm::vec3(0.5f, 0.0f, 0.0f), glm::vec3(0), glm::vec3(0), 1.0f, 0.5f)); // 1
    bodies.push_back(Body(glm::vec3(0.0f, 0.5f, 0.0f), glm::vec3(0), glm::vec3(0), 1.0f, 0.5f)); // 2

    // --- 群組 B：和 A 完全不相鄰 ---
    bodies.push_back(Body(glm::vec3(5.0f, 5.0f, 5.0f), glm::vec3(0), glm::vec3(0), 1.0f, 0.5f)); // 3
    bodies.push_back(Body(glm::vec3(5.5f, 5.0f, 5.0f), glm::vec3(0), glm::vec3(0), 1.0f, 0.5f)); // 4

    // --- 邊界案例：剛好在 searchRadius 上 ---
    bodies.push_back(Body(glm::vec3(0.0f, 0.999f, 0.0f), glm::vec3(0), glm::vec3(0), 1.0f, 0.5f)); // 5 ✓ 勉強在內
    bodies.push_back(Body(glm::vec3(0.0f, 1.001f, 0.0f), glm::vec3(0), glm::vec3(0), 1.0f, 0.5f)); // 6 ✗ 剛好在外

    // --- 負座標：測試 floor 是否正確 ---
    bodies.push_back(Body(glm::vec3(-0.3f, 0.0f, 0.0f), glm::vec3(0), glm::vec3(0), 1.0f, 0.5f)); // 7
}

void Simulation::GenerateRandom(int n, float rangeMin, float rangeMax) {
    bodies.clear();
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> posDist(rangeMin, rangeMax);
    std::uniform_real_distribution<float> massDist(0.5f, 2.0f);
    std::uniform_real_distribution<float> radiusDist(0.1f, 0.3f);


    for (int i = 0; i < n; i++) {
        glm::vec3 pos(posDist(rng), posDist(rng), posDist(rng));
        float mass   = massDist(rng);
        float radius = radiusDist(rng);
        bodies.push_back(Body(pos, glm::vec3(0), glm::vec3(0), mass, radius));
    }

}
// B

// 1. Brute Force
std::vector<NeighborPair> Simulation::BruteForce() {
    if (bodies.empty()) return {};
    
    std::vector<NeighborPair> pairs;
    float h2 = searchRadius * searchRadius;

    for (int i = 0; i < (int)bodies.size(); i++) {
        for (int j = i + 1; j < (int)bodies.size(); j++) {
            glm::vec3 d = bodies[j].pos - bodies[i].pos;
            float dist2 = d.x*d.x + d.y*d.y + d.z*d.z;
            if (dist2 > h2) continue;
            pairs.push_back({i, j, dist2});
        }
    }
    return pairs;
}

// 2. Uniform Grid
#define NUM_OF_CELL 1024
struct Cell {
    Cell (int posx, int posy, int posz): pos(posx, posy, posz){}
    glm::ivec3 pos;
};
int ComputeHashBucketIndex(Cell cell) {
    const int h1 = 0x8da6b343;
    const int h2 = 0xd8163841;
    const int h3 = 0xcb1ab31f;
    int n = h1 * cell.pos.x + h2 * cell.pos.y + h3 * cell.pos.z;
    n = n % NUM_OF_CELL;
    if (n < 0) n += NUM_OF_CELL;
    return n;
}
std::vector<NeighborPair> Simulation::UniformGrid() {
    std::array<std::vector<int>, NUM_OF_CELL> table;

    for (int i = 0; i < (int)bodies.size(); i++) {
        int ix = (int)std::floor(bodies[i].pos.x / searchRadius);
        int iy = (int)std::floor(bodies[i].pos.y / searchRadius);
        int iz = (int)std::floor(bodies[i].pos.z / searchRadius);
        Cell cell(ix, iy, iz);
        int bucket = ComputeHashBucketIndex(cell);
        table[bucket].push_back(i);
        //std::cout << "body "<<i<< ":[" << bucket << "]\n";
    }

    

    std::vector<NeighborPair> pairs;
    float h2 = searchRadius * searchRadius;

    for (int i = 0; i < (int)bodies.size(); i++) {
        int ix = (int)std::floor(bodies[i].pos.x / searchRadius);
        int iy = (int)std::floor(bodies[i].pos.y / searchRadius);
        int iz = (int)std::floor(bodies[i].pos.z / searchRadius);

        
        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                for (int dz = -1; dz <= 1; dz++) {
                    Cell neighbor(ix+dx, iy+dy, iz+dz);
                    int bucket = ComputeHashBucketIndex(neighbor);
                    for (int j : table[bucket]) {
                        if (j <= i) continue; 
                        glm::vec3 d = bodies[j].pos - bodies[i].pos;
                        float dist2 = d.x*d.x + d.y*d.y + d.z*d.z;
                        if (dist2 > h2) continue;
                        pairs.push_back({i, j, dist2});
                    }
                }
            }
        }
    }
    return pairs;

}



// 3. Octree

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
                << "DISTANCE: {" << std::fixed << std::setprecision(4) << std::sqrt(pair.distance2)<< "};\n";
    }
}