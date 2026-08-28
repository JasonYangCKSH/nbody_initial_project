#include <iostream>
#include <vector>
#include <algorithm>
#include <random>
#include <cassert>
#include <cmath>

// 引入你的 Particle 標頭檔與修正後的 broad_phase.h
#include "particle.h"
#include "broad_phase.h"

// -----------------------------------------------------------------------------
// 1. 規範化 PairList (Canonical Normalization)
// 確保 PairList 內部每一對 (a, b) 都滿足 a < b，且整體依 a 再依 b 排序
// -----------------------------------------------------------------------------
void normalizePairList(PairList& pairs) {
    for (auto& pair : pairs) {
        if (pair.first > pair.second) {
            std::swap(pair.first, pair.second);
        }
    }
    std::sort(pairs.begin(), pairs.end(), [](const auto& p1, const auto& p2) {
        if (p1.first != p2.first) return p1.first < p2.first;
        return p1.second < p2.second;
    });
    // 移除重複的 pair
    pairs.erase(std::unique(pairs.begin(), pairs.end()), pairs.end());
}

// -----------------------------------------------------------------------------
// 2. 測試資料生成器 (Random / Grid Edge-Case Particles)
// -----------------------------------------------------------------------------
std::vector<Particle> generateTestParticles(size_t count, float worldSize) {
    std::vector<Particle> particles(count);
    std::mt19937 rng(42); // 固定隨機種子，確保測試可複現 (Deterministic)
    std::uniform_real_distribution<float> posDist(-worldSize, worldSize);
    std::uniform_real_distribution<float> radiusDist(0.1f, 0.5f);
    std::uniform_real_distribution<float> skinDist(0.01f, 0.05f);

    for (size_t i = 0; i < count; ++i) {
        particles[i].pos = glm::vec3(posDist(rng), posDist(rng), posDist(rng));
        particles[i].radius = radiusDist(rng);
        particles[i].skin = skinDist(rng);
        
        // 刻意製造極端狀況：讓部分粒子剛好踩在原點 0.0 或格點邊界上
        if (i % 50 == 0) particles[i].pos.x = 0.0f;
        if (i % 50 == 1) particles[i].pos.y = 0.0f;
    }
    return particles;
}

// -----------------------------------------------------------------------------
// 3. 比對與除錯印出 (Diff & Debug Report)
// -----------------------------------------------------------------------------
bool verifyPairLists(const PairList& bfPairs, const PairList& gridPairs) {
    if (bfPairs == gridPairs) {
        std::cout << " Validation PASSED! Both algorithms found " 
                  << bfPairs.size() << " identical collision pairs.\n";
        return true;
    }

    std::cout << " Validation FAILED!\n";
    std::cout << "  - Brute Force Pair Count : " << bfPairs.size() << "\n";
    std::cout << "  - Uniform Grid Pair Count: " << gridPairs.size() << "\n\n";

    // 找出 Brute Force 有，但 Uniform Grid 沒搜到的 (漏報 False Negative)
    std::cout << "--- Missing Pairs in Uniform Grid (False Negatives) ---\n";
    int missingCount = 0;
    for (const auto& pair : bfPairs) {
        if (!std::binary_search(gridPairs.begin(), gridPairs.end(), pair)) {
            std::cout << "  Missing Pair: (" << pair.first << ", " << pair.second << ")\n";
            if (++missingCount >= 10) { 
                std::cout << "  ... (truncated)\n"; 
                break; 
            }
        }
    }

    // 找出 Uniform Grid 有，但 Brute Force 沒搜到的 (誤報 False Positive)
    std::cout << "--- Extra Pairs in Uniform Grid (False Positives) ---\n";
    int extraCount = 0;
    for (const auto& pair : gridPairs) {
        if (!std::binary_search(gridPairs.begin(), gridPairs.end(), pair)) {
            std::cout << "  Extra Pair: (" << pair.first << ", " << pair.second << ")\n";
            if (++extraCount >= 10) { 
                std::cout << "  ... (truncated)\n"; 
                break; 
            }
        }
    }

    return false;
}

// -----------------------------------------------------------------------------
// 4. 主測試程式入口
// -----------------------------------------------------------------------------
int main() {
    std::cout << "===========================================\n";
    std::cout << " Starting Broad-Phase Verification Harness \n";
    std::cout << "===========================================\n\n";

    // A. 建立測試粒子資料
    const size_t NUM_PARTICLES = 2000;
    const float WORLD_SIZE = 50.0f;
    const bool WITH_SKIN = true;

    auto particles = generateTestParticles(NUM_PARTICLES, WORLD_SIZE);

    // B. 執行 Brute Force 取得基準答案 (Ground Truth)
    std::cout << "[1/2] Running Brute Force Baseline...\n";
    PairList bfPairs = broad::BruteForce(particles, WITH_SKIN);
    normalizePairList(bfPairs);

    // C. 執行 Uniform Grid 取得測試結果
    std::cout << "[2/2] Running Uniform Grid (Morton Code)...\n";
    
    // 計算安全 Cell Size: 必須 >= 2 * (Max Radius + Max Skin)
    // 這裡 maxRadius = 0.5f, maxSkin = 0.05f，最大兩粒子碰撞距離和為 2 * (0.55) = 1.1f
    const float SAFE_CELL_SIZE = 1.2f; 
    broad::UniformGrid grid(SAFE_CELL_SIZE);
    
    PairList gridPairs = grid.Build(particles, WITH_SKIN);
    normalizePairList(gridPairs);

    // D. 驗證兩者是否 100% 匹配
    std::cout << "\n[Verifying Results...]\n";
    bool success = verifyPairLists(bfPairs, gridPairs);

    return success ? 0 : -1;
}