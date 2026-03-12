#include <iostream>
#include <vector>
#include <random>
#include <chrono>
using namespace std;

struct Particle
{
    double x, y, z;
    double vx, vy, vz;
};

int main()
{
    const size_t N = 10000000;
    const double dt = 0.01;

    std::vector<Particle> particles(N);

    // 初始化
    std::mt19937 rng(42);
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    auto start = chrono::high_resolution_clock::now();
    for (size_t i = 0; i < N; i++)
    {
        particles[i].x = dist(rng);
        //particles[i].y = dist(rng);
        //particles[i].z = dist(rng);

        particles[i].vx = dist(rng);
        //particles[i].vy = dist(rng);
        //particles[i].vz = dist(rng);
    }
    auto end = chrono::high_resolution_clock::now();
    // update position
    auto start2 = chrono::high_resolution_clock::now();
    for (size_t i = 0; i < N; i++)
    {
        particles[i].x += particles[i].vx * dt;
        //particles[i].y += particles[i].vy * dt;
        //particles[i].z += particles[i].vz * dt;
    }
    auto end2 = chrono::high_resolution_clock::now();
    chrono::duration<double> dur = end - start;
    chrono::duration<double> dur2 = end2 - start2;
    std::cout << dur.count() <<", "<< dur2.count()<< std::endl;
}