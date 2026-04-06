#include <iostream>
#include <chrono>
#include <vector>
#include <set>
#include <algorithm>
#include <iomanip>
#include <glm/glm.hpp>

#include "body.hpp"
#include "Barnes-HutOctree.hpp"
#include "simulation.hpp"
#include "senario.hpp"

// --- 輔助工具函式 ---

// 取得當前時間
auto now() { return std::chrono::high_resolution_clock::now(); }

// 計算毫秒差
double ms(std::chrono::high_resolution_clock::time_point start, std::chrono::high_resolution_clock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}

// 進度條顯示
void PrintProgress(int current, int total, int bar_width = 20) {
    float percent = (float)current / (float)total;
    int filled = (int)(bar_width * percent);
    std::cout << "\r[";
    for (int i = 0; i < bar_width; i++) std::cout << (i < filled ? '=' : ' ');
    std::cout << "] " << (int)(percent * 100) << "% (" << current << "/" << total << " frames)";
    std::cout.flush();
}

// 將碰撞對標準化並轉為 set，以便進行精確比對
// 格式化為 std::pair<int, int> 並確保 first < second
std::set<std::pair<int, int>> NormalizePairs(const std::vector<NeighborPair>& pairs) {
    std::set<std::pair<int, int>> s;
    for (const auto& p : pairs) {
        int a = p.i;
        int b = p.j;
        if (a > b) std::swap(a, b);
        s.insert({a, b});
    }
    return s;
}

// --- 主程式 ---

int main() {
    // 實驗參數設定
    int N = 2000;          // 驗證模式下 N 不建議太大（因 Brute Force 較慢）
    float range = 100.0f;
    float mass = 1.0f;
    float radius = 0.5f;
    float dt = 0.01f;
    float theta = 0.5f;
    float epsilon = 0.1f;
    int test_frames = 100;

    while (true) {
        std::cout << "\n\n==========================================" << std::endl;
        std::cout << "   N-Body Collision Verification Bench    " << std::endl;
        std::cout << "==========================================" << std::endl;
        
        // 1. 選擇測試演算法
        std::cout << "Select Test Algorithm:\n(1) Brute Force (Self-Test)\n(2) Uniform Grid\n(3) Octree\n(0) Exit\nChoice: ";
        int mode; std::cin >> mode;
        if (mode == 0) break;

        NeighborMethod test_method;
        std::string method_name;
        if (mode == 1) { test_method = NeighborMethod::BRUTE_FORCE; method_name = "Brute Force"; }
        else if (mode == 2) { test_method = NeighborMethod::UNIFORM_GRID; method_name = "Uniform Grid"; }
        else { test_method = NeighborMethod::OCTREE; method_name = "Octree"; }

        // 2. 選擇測試情境
        std::cout << "\nSelect Scenario:\n(1) Uniform Random\n(2) Clustered\n(3) Extreme Clustered\n(4) Normal Test Bench\nChoice: ";
        int sc_mode; std::cin >> sc_mode;
        
        std::vector<Body> bodies;
        if (sc_mode == 1) bodies = Senario::UniformRandom(N, range, mass, radius);
        else if (sc_mode == 2) bodies = Senario::Clustered(N, 10, range, 5.0f, mass, radius);
        else if (sc_mode == 3) bodies = Senario::ExtremeClustered(N, mass, radius);
        else if (sc_mode == 4) {
            //N = 5;
            bodies = Senario::NormalTestBench();
        }
        // 3. 初始化兩套完全同步的模擬系統
        // sim_ref 是「實驗組」(永遠用 Brute Force)
        // sim_test 是「對照組」(用你選的優化演算法)
        Simulation sim_ref(dt, theta, epsilon, bodies, NeighborMethod::BRUTE_FORCE);
        Simulation sim_test(dt, theta, epsilon, bodies, test_method);

        std::cout << "\n--- Starting Verification & Benchmarking ---" << std::endl;
        std::cout << "N: " << N << " | Method: " << method_name << std::endl;

        double total_test_time = 0;
        bool accuracy_passed = true;
        int error_frame = -1;

        for (int i = 0; i < test_frames; i++) {
            // A. 正確性比對 (在執行 step 之前，先看兩者找出的 Pairs 是否相同)
            auto pairs_ref = sim_ref.get_neighbor_pairs();
            auto pairs_test = sim_test.get_neighbor_pairs();

            auto set_ref = NormalizePairs(pairs_ref);
            auto set_test = NormalizePairs(pairs_test);

            if (set_ref != set_test && accuracy_passed) {
                accuracy_passed = false;
                error_frame = i;
                //std::cout << "=========error!=======\n";
                //break;
                
            }

            // B. 效能測量 (測量對照組的 step 耗時)
            auto t_start = now();
            sim_test.step();
            auto t_end = now();
            total_test_time += ms(t_start, t_end);

            // C. 實驗組同步 (必須也跑 step，確保下一幀兩者的粒子座標依然一致)
            sim_ref.step();

            PrintProgress(i + 1, test_frames);
        }

        // 4. 輸出結果報告
        std::cout << "\n\n---------------- Result ----------------" << std::endl;
        if (accuracy_passed) {
            std::cout << "Accuracy: [ PASSED ] (Matches Brute Force 100%)" << std::endl;
        } else {
            std::cout << "Accuracy: [ FAILED ] (Mismatch first found at Frame " << error_frame << ")" << std::endl;
        }

        std::cout << "Total Time (" << method_name << "): " << total_test_time << " ms" << std::endl;
        std::cout << "Avg Time per Frame: " << total_test_time / test_frames << " ms" << std::endl;
        std::cout << "----------------------------------------" << std::endl;
    }

    return 0;
}