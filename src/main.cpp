#include <iostream>
#include <vector>
#include <random>
#include <iomanip>
#include <chrono>
#include "octree.h"
int main() {
    
    const int N = 1000000, STEPS = 100; 
    const float DT = 0.01f, THETA = 1.0f, EPSILON = 0.01f;
    const float G = 1.0f;
    // bodies list
    std::vector<Body> bodies;
    bodies.resize(N);

    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dist(-100.f, 100.f);
    
    for (auto& b : bodies) {
        b.pos = {dist(gen), dist(gen), dist(gen)};
        b.mass = 1.0f; b.vel = {0.f, 0.f, 0.f};
    
    }

    // initialize an octree
    Octree tree(THETA, EPSILON);
    Oct boundary;
    boundary.new_containing(bodies);

    auto start = std::chrono::high_resolution_clock::now();
    
    // ----build the octree----
    tree.clear(boundary);
    for (const auto& b: bodies){
        tree.insert(b.pos, b.mass);
    }
    tree.propagate();
    // ------------------------


    for (auto& b : bodies) {
        glm::vec3 acc = tree.calculate_acc(b.pos) * G;
        b.vel += acc * DT;
        b.pos += b.vel * DT;
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;

    std::cout << "Particles: " << N << "\nNodes: " << tree.nodes.size() << "\n";
    std::cout << "Root Mass: " << tree.nodes[0].total_mass << " (Check sum)\n";
    std::cout << "Execution Time: " << elapsed.count() << " ms\n";
    return 0;
}