#include <iostream>
#include <chrono>
#include <fstream>
#include <glm/glm.hpp>
#include "body.hpp"
#include "Barnes-HutOctree.hpp"
#include "simulation.hpp"
#include "senario.hpp"
std::chrono::time_point<std::chrono::high_resolution_clock> now() {
    return std::chrono::high_resolution_clock::now();
}
double ms(std::chrono::high_resolution_clock::time_point start, std::chrono::high_resolution_clock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}
void PrintProgress(int current, int total, int bar_width = 20) {
    float percent = (float) current / (float) total;     
    int filled =  (int)(bar_width * percent);                      

    std::cout << "\r[";                     
    for (int i = 0; i < bar_width; i++)
        std::cout << (i < filled ? '=' : ' ');
    std::cout << "] " << (int)(percent * 100) << "% ("
              << current << "/" << total << " frames)";
    std::cout.flush();                       
}
int main() {
    // position range: ([-100, 100], [-100, 100], [-100, 100])
    
    int N = 10000;
    float range = 100;
    float mass = 1.0f;
    float radius = 1.0f;

    float dt = 0.01f;
    float theta = 0.5f;
    float epsilon = 0.1f;
    
    NeighborMethod neighbor_method = NeighborMethod::UNIFORM_GRID;
    std::vector<Body> bodies;

    // choose a senario to form the example test bench
    bodies = Senario::UniformRandom(N, range, mass, radius);
    
    // start to simulate the moving part
    Simulation sim(dt, theta, epsilon, bodies, neighbor_method);
    std::cout << "---simulation started---\n";
    std::cout << "N: " << N << std::endl;
    int frame = 100;
    std::cout << "Frame: " << frame << std::endl;
    auto start = now();
    for (int i = 0; i < frame; i++) {
        sim.step();
        PrintProgress(i + 1, frame);
    }
    auto end = now();
    std::cout << "\ntime spend: "<< ms(start, end) << " ms\n";
    std::cout << "---simulation ended---\n";
    return 0;
}