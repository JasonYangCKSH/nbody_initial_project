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
    SimulationConfig(
        float particleRadius = 1.0,
        float dt = 1.0f / 60.0f, float K = 2.0f, bool hasSkin = false,
        Method method = Method::UniformGrid, float cellSize = 2.0f,
        int maxDepth = 8, int leafCapacity = 8, float worldSize = 100.0f
    ): particleRadius_(particleRadius),dt(dt),K(K),hasSkin(hasSkin),method(method),cellSize(cellSize),
       maxDepth(maxDepth),leafCapacity(leafCapacity),worldSize(worldSize){
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
