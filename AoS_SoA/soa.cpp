#include <iostream>
#include <vector>
#include <random>
#include <chrono>
using namespace std;
struct Particles
{
    std::vector<double> x, y, z;
    std::vector<double> vx, vy, vz;

    Particles(size_t N)
    {
        x.resize(N);
        y.resize(N);
        z.resize(N);
        vx.resize(N);
        vy.resize(N);
        vz.resize(N);
    }
};

int main()
{
    const size_t N = 10000000;
    const double dt = 0.01;

    Particles p(N);

    std::mt19937 rng(42);
    std::uniform_real_distribution<double> dist(0.0, 1.0);

    // 初始化
    for (size_t i = 0; i < N; i++)
    {
        p.x[i] = dist(rng);
        p.y[i] = dist(rng);
        p.z[i] = dist(rng);

        p.vx[i] = dist(rng);
        p.vy[i] = dist(rng);
        p.vz[i] = dist(rng);
    }

    auto start = chrono::high_resolution_clock::now();
    // update position
    for (size_t i = 0; i < N; i++)
    {
        p.x[i] += p.vx[i] * dt;
        p.y[i] += p.vy[i] * dt;
        p.z[i] += p.vz[i] * dt;
    }
    auto end = chrono::high_resolution_clock::now();
    chrono::duration<double> dur = end - start;
    std::cout << dur.count() << std::endl;
}