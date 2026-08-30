#include <chrono>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "scenario.h"
#include "simulation.h"

namespace {

float readFloat(const std::string& prompt) {
    std::cout << prompt;
    float value;
    while (!(std::cin >> value)) {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "輸入無效，請重新輸入數字: ";
    }
    return value;
}

int readInt(const std::string& prompt) {
    std::cout << prompt;
    int value;
    while (!(std::cin >> value)) {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "輸入無效，請重新輸入整數: ";
    }
    return value;
}

struct BenchResult {
    std::string name;
    bool hasPhaseBreakdown = false;
    bool hasRebuildCount = false;
    double totalSeconds = 0.0;
    double broadPhaseSeconds = 0.0;
    double narrowPhaseSeconds = 0.0;
    int rebuildCount = 0;
};

void printResult(const BenchResult& r) {
    std::cout << "\n[" << r.name << "]\n";
    std::cout << "  total time        : " << r.totalSeconds << " s\n";
    if (r.hasPhaseBreakdown) {
        std::cout << "  broad-phase time  : " << r.broadPhaseSeconds << " s\n";
        std::cout << "  narrow-phase time : " << r.narrowPhaseSeconds << " s\n";
    }
    if (r.hasRebuildCount) {
        std::cout << "  rebuild count     : " << r.rebuildCount << "\n";
    }
}

BenchResult runOne(const std::string& name, StructureMode mode, bool skinEnabled,
                    const std::vector<Particle>& initial, const SimConfig& cfg,
                    int frames, int leafCapacity, int maxDepth, float worldSize) {
    std::unique_ptr<std::variant<broad::UniformGrid, broad::Octree>> structure;
    if (mode == StructureMode::UniformGrid) {
        structure = std::make_unique<std::variant<broad::UniformGrid, broad::Octree>>(
            broad::UniformGrid(cfg.cellSize));
    } else if (mode == StructureMode::Octree) {
        structure = std::make_unique<std::variant<broad::UniformGrid, broad::Octree>>(
            broad::Octree(maxDepth, leafCapacity, worldSize));
    }

    Simulation sim(frames, mode, std::move(structure), skinEnabled, cfg);
    sim.InitializeParticles(initial);

    auto start = std::chrono::high_resolution_clock::now();
    auto history = sim.runForFrames(frames);
    auto end = std::chrono::high_resolution_clock::now();

    BenchResult result;
    result.name = name;
    result.totalSeconds = std::chrono::duration<double>(end - start).count();
    result.hasPhaseBreakdown = (mode != StructureMode::BruteForce);
    result.hasRebuildCount = skinEnabled;

    for (const auto& s : history) {
        result.broadPhaseSeconds += s.broadPhaseSeconds;
        result.narrowPhaseSeconds += s.narrowPhaseSeconds;
        if (s.rebuild) ++result.rebuildCount;
    }

    return result;
}

}  // namespace

int main() {
    std::cout << "=== Collision Benchmark ===\n\n";

    SimConfig cfg;
    cfg.K = readFloat("K (碰撞回應剛性係數): ");
    cfg.dt = readFloat("dt (每個 time frame 的長度, 秒): ");
    int frames = readInt("total frame number (模擬總幀數): ");

    std::cout << "\n選擇 scenario:\n"
                 "  1) two-particle bounce (固定 2 顆粒子)\n"
                 "  2) uniform cloud\n"
                 "  3) explosion\n";
    int scenarioChoice = readInt("scenario 選擇 (1-3): ");

    int particleCount = 2;
    if (scenarioChoice == 2 || scenarioChoice == 3) {
        particleCount = readInt("粒子數: ");
    }

    cfg.cellSize = readFloat("cellSize (uniform grid 用): ");
    int leafCapacity = readInt("leafCapacity (octree 用): ");
    int maxDepth = readInt("maxDepth (octree 用): ");

    float particleRadius = 0.5f;
    if (scenarioChoice == 2 || scenarioChoice == 3) {
        particleRadius = readFloat("particle size (半徑): ");
    }

    constexpr float kBoxSize = 100.0f;
    constexpr float kSpeed = 2.0f;
    constexpr unsigned kSeed = 42;
    const float worldSize = kBoxSize * 2.0f;

    std::vector<Particle> initial;
    switch (scenarioChoice) {
        case 2:
            initial = scenario::uniformCloud(particleCount, kBoxSize, particleRadius, kSpeed, kSeed);
            break;
        case 3:
            initial = scenario::explosion(particleCount, kBoxSize, particleRadius, kSpeed, kSeed);
            break;
        default:
            initial = scenario::two_particle_bounce_scenario();
            break;
    }

    std::cout << "\n開始執行 " << frames << " frames, 粒子數 = " << initial.size() << " ...\n";

    std::vector<BenchResult> results;
    results.push_back(runOne("brute force", StructureMode::BruteForce, false, initial, cfg,
                              frames, leafCapacity, maxDepth, worldSize));
    results.push_back(runOne("uniform grid", StructureMode::UniformGrid, false, initial, cfg,
                              frames, leafCapacity, maxDepth, worldSize));
    results.push_back(runOne("uniform grid + skin", StructureMode::UniformGrid, true, initial,
                              cfg, frames, leafCapacity, maxDepth, worldSize));
    results.push_back(runOne("octree", StructureMode::Octree, false, initial, cfg, frames,
                              leafCapacity, maxDepth, worldSize));
    results.push_back(runOne("octree + skin", StructureMode::Octree, true, initial, cfg, frames,
                              leafCapacity, maxDepth, worldSize));

    std::cout << std::fixed << std::setprecision(6);
    std::cout << "\n=== 結果 ===\n";
    for (const auto& r : results) {
        printResult(r);
    }

    return 0;
}
