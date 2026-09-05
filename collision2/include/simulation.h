#pragma once

#include <algorithm>
#include <chrono>
#include <variant>
#include <vector>
#include "scenario.h"
#include "particle.h"
#include "broad_phase.h"
#include "narrow_phase.h"
#include "verlet_buffer.h"
#include "brute_force.h"
#include "collision_response.h"

enum class Method {
    BruteForce,    // 單一獨立基準方法，不參與 broad-phase / narrow-phase 設定
    UniformGrid,
    Octree,
};
struct SimulationConfig {
    float particleRadius_;
    float dt;
    float K;
    bool hasSkin;
    Method method;
    float worldSize;
    float cellSize;
    int maxDepth;
    int leafCapacity;
    //=======
    float cellSizeRatio;
    //=======
    // 只有需要驗證 broad-phase candidate 集合本身（例如確認 candidatePairs 有沒有
    // 涵蓋所有 brute force ground truth 碰撞、不是只比對最終 collisionPairs）時才打開。
    // 預設關閉：candidatePairs 沒有任何地方在讀，開著只是白白讓 FrameInfo 每幀複製
    // 一份完整 candidate list，K 越大 candidate 越多，複製成本會蓋過真正的演算法時間。
    bool recordCandidatePairs = false;
    SimulationConfig(
        float particleRadius = 1.0,
        float dt = 1.0f / 60.0f, float K = 2.0f, bool hasSkin = false,
        Method method = Method::UniformGrid, float cellSize = 2.0f,
        int maxDepth = 8, int leafCapacity = 8, float worldSize = 100.0f,
        bool recordCandidatePairs = false
    ): particleRadius_(particleRadius),dt(dt),K(K),hasSkin(hasSkin),method(method),cellSize(cellSize),
       maxDepth(maxDepth),leafCapacity(leafCapacity),worldSize(worldSize),recordCandidatePairs(recordCandidatePairs){
        cellSizeRatio = cellSize / (2 * particleRadius);
    }

};

struct FrameInfo {
    int frameIndex;
    bool didRebuild;
    double broadPhaseTime;
    double narrowPhaseTime;
    double responsePhaseTime;
    PairList candidatePairs;
    PairList collisionPairs;
    size_t candidateCountCache = 0;
    size_t collisionCountCache = 0;
    size_t candidateCount() const { return candidateCountCache; }
    size_t collisionCount() const { return collisionCountCache; }

};

class Simulation {
private:
    std::vector<Particle> particles_;
    int total_frame_;
    int rebuildCount_;
    std::vector<FrameInfo> fi_;
    SimulationConfig cfg_;
    int currentFrame_ = 0;
    std::variant<broad::UniformGrid, broad::Octree> broadPhase_;
    PairList cachedCandidates_;
    bool needsRebuild() const;
    PairList buildBroadPhase() const;
    void updateSkin();
    void integrate();
    void applyCollisionResponse(const PairList& collisions);
    FrameInfo step();

public:
    Simulation(SimulationConfig cfg);
    void initialize(std::vector<Particle> particles, int totalFrames);
    void run();
    const std::vector<Particle>& particles() const { return particles_; }
    const SimulationConfig& config() const { return cfg_; }
    int currentFrame() const { return currentFrame_; }
    int totalFrames() const { return total_frame_; }
    int rebuildCount() const { return rebuildCount_; }
    const std::vector<FrameInfo>& frameHistory() const { return fi_; }
    const FrameInfo& lastFrameInfo() const { return fi_.back(); }
};
