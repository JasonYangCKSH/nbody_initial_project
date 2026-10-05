#include "simulation.h"

Simulation::Simulation(SimulationConfig cfg)
    : cfg_(cfg),
      broadPhase_(broad::UniformGrid(cfg.cellSize)) {
    if (cfg_.method == Method::Octree) {
        broadPhase_ = broad::Octree(cfg_.maxDepth, cfg_.leafCapacity, cfg_.worldSize);
    }
}

void Simulation::initialize(std::vector<Particle> particles, int totalFrames) {
    particles_ = std::move(particles);
    total_frame_ = totalFrames;
    currentFrame_ = 0;
    rebuildCount_ = 0;
    cachedCandidates_.clear();
    fi_.clear();
    fi_.reserve(totalFrames);
}

void Simulation::run() {
    for (int i = 0; i < total_frame_; ++i) {
        fi_.push_back(step());
    }
}

bool Simulation::needsRebuild() const {
    return currentFrame_ == 0 || !verlet::listStillValid(particles_);
}


void Simulation::rebuildBroadPhase() {
    if (auto* grid = std::get_if<broad::UniformGrid>(&broadPhase_)) {
        auto builtGrid = grid->BuildGrid(particles_);
        if (cfg_.hasSkin) {
            verlet::updateLocalSkin(particles_, cfg_.K, cfg_.dt);
            verlet::capSkinToCellSize(particles_, cfg_.cellSize);
        }
        cachedCandidates_ = grid->CollectPairs(builtGrid, particles_, cfg_.hasSkin);
    } else if (auto* octree = std::get_if<broad::Octree>(&broadPhase_)) {
        auto tree = octree->BuildTree(particles_);
        if (cfg_.hasSkin) {
            verlet::updateLocalSkin(particles_, cfg_.K, cfg_.dt);
            verlet::capSkinToLeafExtent(particles_, octree->LeafHalfExtents(tree, particles_));
        }
        cachedCandidates_ = octree->CollectPairs(tree, particles_, cfg_.hasSkin);
    }
}

void Simulation::integrate() {
    for (auto& p : particles_) {
        p.vel += p.acc * cfg_.dt;
        p.pos += p.vel * cfg_.dt;
    }
    response::reflectOffWalls(particles_, cfg_.worldSize);
}

void Simulation::applyCollisionResponse(const PairList& collisions) {
    response::resolveCollisions(particles_, collisions);
}

FrameInfo Simulation::step() {
    FrameInfo info;
    info.frameIndex = currentFrame_;

    PairList collisions;

    if (cfg_.method == Method::BruteForce) {
        auto t0 = std::chrono::steady_clock::now();
        PairList brutePairs = bruteforce::BruteForce(particles_);
        auto t1 = std::chrono::steady_clock::now();

        info.didRebuild = false;
        info.broadPhaseTime = 0.0;
        info.narrowPhaseTime = std::chrono::duration<double, std::milli>(t1 - t0).count();
        info.candidateCountCache = brutePairs.size();
        if (cfg_.recordCandidatePairs) info.candidatePairs = brutePairs;

        collisions = std::move(brutePairs);
    } else {
        info.didRebuild = needsRebuild();

        if (info.didRebuild) {
            auto t0 = std::chrono::steady_clock::now();
            rebuildBroadPhase();
            ++rebuildCount_;

            verlet::recordBroadPhaseSnapshot(particles_);
            auto t1 = std::chrono::steady_clock::now();
            info.broadPhaseTime = std::chrono::duration<double, std::milli>(t1 - t0).count();
        } else {
            info.broadPhaseTime = 0.0;
        }

        auto t2 = std::chrono::steady_clock::now();
        for (const auto& [i, j] : cachedCandidates_) {
            if (narrow::colliding(particles_[i], particles_[j])) {
                collisions.emplace_back(i, j);
            }
        }
        auto t3 = std::chrono::steady_clock::now();
        info.narrowPhaseTime = std::chrono::duration<double, std::milli>(t3 - t2).count();

        info.candidateCountCache = cachedCandidates_.size();
        if (cfg_.recordCandidatePairs) info.candidatePairs = cachedCandidates_;
    }

    
    std::sort(collisions.begin(), collisions.end());
    auto t4 = std::chrono::steady_clock::now();
    applyCollisionResponse(collisions);
    integrate();
    auto t5 = std::chrono::steady_clock::now();
    info.responsePhaseTime = std::chrono::duration<double, std::milli>(t5 - t4).count();

    info.collisionCountCache = collisions.size();
    info.collisionPairs = collisions;

    ++currentFrame_;
    return info;
}
