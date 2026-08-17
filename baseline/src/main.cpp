#include <iostream>
#include <chrono>
#include <fstream>
#include <vector>
#include <numeric>
#include <algorithm>
#include "simulation.hpp"

const int NUM_RUNS = 5; // 每個 bodyNum 跑幾次取平均

double measureTime(std::function<void()> func) {
    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    return elapsed.count();
}

int main() {

    Simulation sim(1.0f);
    const std::vector<int> smallBodyNumVec  = {1000,  2000,  3000,  4000,  5000,  6000,  7000,  8000,  9000,  10000};
    const std::vector<int> mediumBodyNumVec = {10000, 20000, 30000, 40000, 50000, 60000, 70000, 80000, 90000, 100000};
    const std::vector<int> largeBodyNumVec  = {100000, 200000, 300000, 400000, 500000, 600000, 700000, 800000, 900000, 1000000};
    std::vector<int> bodyNumVec = smallBodyNumVec;

    //std::ofstream of1("../graph/bruteforce.csv");
    std::ofstream of2("../graph/uniformgrid.csv");
    std::ofstream of3("../graph/octree.csv");

    for (const int& bodyNum : bodyNumVec) {
        std::cout << "\n========== Body Num: " << bodyNum << " ==========\n";

        std::vector<double> bf_times, ug_times, ot_times;

        for (int run = 0; run < NUM_RUNS; run++) {
            std::cout << "Run " << run + 1 << "/" << NUM_RUNS << "\n";

            sim.GenerateRandom(bodyNum, -15.0f, 15.0f);
            sim.GenerateNonUniform(bodyNum, -15.0f, 15.0f);
            // Brute Force
            //std::vector<NeighborPair> pairs;
            //double bf_t = measureTime([&]() { pairs = sim.BruteForce(); });
            //bf_times.push_back(bf_t);

            // Uniform Grid
            std::vector<NeighborPair> pairs2;
            double ug_t = measureTime([&]() { pairs2 = sim.UniformGrid(); });
            ug_times.push_back(ug_t);
            //assert(pairs.size() == pairs2.size());

            // Octree
            std::vector<NeighborPair> pairs3;
            double ot_t = measureTime([&]() { pairs3 = sim.Octree(); });
            ot_times.push_back(ot_t);
            //assert(pairs.size() == pairs3.size());
        }

        // 計算平均（去掉最大最小值）
        auto trimmed_mean = [](std::vector<double> v) {
            std::sort(v.begin(), v.end());
            v.erase(v.begin());        // 去掉最小
            v.erase(v.end() - 1);     // 去掉最大
            double sum = std::accumulate(v.begin(), v.end(), 0.0);
            return sum / v.size();
        };

        //double bf_avg = trimmed_mean(bf_times);
        double ug_avg = trimmed_mean(ug_times);
        double ot_avg = trimmed_mean(ot_times);

        //std::cout << "Brute Force  avg: " << bf_avg << " ms\n";
        std::cout << "Uniform Grid avg: " << ug_avg << " ms\n";
        std::cout << "Octree       avg: " << ot_avg << " ms\n";

        //of1 << bodyNum << " " << bf_avg << "\n";
        of2 << bodyNum << " " << ug_avg << "\n";
        of3 << bodyNum << " " << ot_avg << "\n";
    }

    //of1.close();
    of2.close();
    of3.close();

    std::cout << "\nDone!\n";
    return 0;
}