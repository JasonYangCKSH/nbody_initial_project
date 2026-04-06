/*#include <iostream>
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
    
    int N = 15000;
    float range = 100.0f;
    float mass = 1.0f;
    float radius = 1.0f;

    float dt = 0.01f;
    float theta = 0.5f;
    float epsilon = 0.1f;
    
    NeighborMethod neighbor_method = NeighborMethod::BRUTE_FORCE;
    std::vector<Body> bodies = Senario::UniformRandom(N, range, mass, radius);
    while(true) {
        bodies.clear();
        int mode;
        std::cout << "please input Data structure(1, 2, 3): ";
        std::cin >> mode;
        if (mode == 1) neighbor_method = NeighborMethod::BRUTE_FORCE;
        if (mode == 2) neighbor_method = NeighborMethod::UNIFORM_GRID;
        if (mode == 3) neighbor_method = NeighborMethod::OCTREE;
        
        

        // choose a senario to form the example test bench
        
        int mode2;
        std::cout << "please input testbench(1, 2, 3): ";
        std::cin >> mode2;
        if (mode2 == 1) {
            bodies = Senario::UniformRandom(N, range, mass, radius);
        }
        if (mode2 == 2) {
            bodies = Senario::Clustered(N, 10, 100.0f, 5.0f, 1.0f, 0.5f);
        }
        if (mode2 == 3) {
            bodies = Senario::ExtremeClustered(N, mass, radius);
        }
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
    }

    return 0;
}*/
#include <iostream>
#include <chrono>
#include <vector>
#include <set>
#include <algorithm>
#include "body.hpp"
#include "simulation.hpp"
#include "senario.hpp"

// 輔助函式：將碰撞對轉化為標準化的 set，方便比對
std::set<std::pair<int, int>> NormalizePairs(const std::vector<std::pair<int, int>>& pairs) {
    std::set<std::pair<int, int>> s;
    for (auto p : pairs) {
        if (p.first > p.second) std::swap(p.first, p.second);
        s.insert(p);
    }
    return s;
}

int main() {
    int N = 1000; // 驗證時 N 不要太大，否則 Brute Force 會跑不動
    float range = 100.0f, mass = 1.0f, radius = 1.0f;
    float dt = 0.01f, theta = 0.5f, epsilon = 0.1f;

    while (true) {
        std::cout << "\n=== Accuracy & Performance Test Bench ===\n";
        std::cout << "1. Uniform Grid vs Brute Force\n";
        std::cout << "2. Octree vs Brute Force\n";
        std::cout << "Select Test Mode (0 to exit): ";
        int mode; std::cin >> mode;
        if (mode == 0) break;

        NeighborMethod test_method = (mode == 1) ? NeighborMethod::UNIFORM_GRID : NeighborMethod::OCTREE;
        
        // 建立兩套完全一樣的初始狀態
        std::vector<Body> bodies = Senario::UniformRandom(N, range, mass, radius);
        
        // 建立兩套模擬器
        Simulation sim_ref(dt, theta, epsilon, bodies, NeighborMethod::BRUTE_FORCE);
        Simulation sim_test(dt, theta, epsilon, bodies, test_method);

        int frames = 50;
        bool all_correct = true;
        double total_time_ref = 0;
        double total_time_test = 0;

        std::cout << "Running verification for " << frames << " frames...\n";

        for (int f = 0; f < frames; f++) {
            // 1. 執行基準組 (Brute Force) 並計時
            auto start_ref = std::chrono::high_resolution_clock::now();
            auto pairs_ref = sim_ref.get_neighbor_pairs(); // 假設你的 Simulation 有這個接口
            sim_ref.step();
            auto end_ref = std::chrono::high_resolution_clock::now();
            total_time_ref += std::chrono::duration<double, std::milli>(end_ref - start_ref).count();

            // 2. 執行測試組 (Grid/Octree) 並計時
            auto start_test = std::chrono::high_resolution_clock::now();
            auto pairs_test = sim_test.get_neighbor_pairs();
            sim_test.step();
            auto end_test = std::chrono::high_resolution_clock::now();
            total_time_test += std::chrono::duration<double, std::milli>(end_test - start_test).count();

            // 3. 正確性比對
            auto set_ref = NormalizePairs(pairs_ref);
            auto set_test = NormalizePairs(pairs_test);

            if (set_ref != set_test) {
                std::cout << "\n[!] Frame " << f << " Error Detected!" << std::endl;
                std::cout << "    Brute Force found: " << set_ref.size() << " pairs" << std::endl;
                std::cout << "    Test Method found: " << set_test.size() << " pairs" << std::endl;
                all_correct = false;
                break; 
            }
            if (f % 10 == 0) std::cout << "Frame " << f << " verified..." << std::endl;
        }

        if (all_correct) {
            std::cout << "\n>>> VERIFICATION SUCCESS! <<<\n";
            std::cout << "Avg Brute Force Time: " << total_time_ref / frames << " ms\n";
            std::cout << "Avg Test Method Time: " << total_time_test / frames << " ms\n";
            std::cout << "Speedup: " << total_time_ref / total_time_test << "x\n";
        } else {
            std::cout << "\n>>> VERIFICATION FAILED! <<<\n";
        }
    }
    return 0;
}