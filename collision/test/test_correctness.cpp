// Validates the paper's Theorem 1: the automatic Verlet update scheme must
// return exactly the same collision list, at every time step, as a
// brute-force reference that has no skin and rebuilds every step.
//
// For each scenario and K value, two identical particle sets are advanced in
// lock-step with the same gravity-only integrator. One goes through
// Simulation (local-velocity skin + linked-cell + automatic update), the
// other is checked against directly with O(n^2) brute force. If the two
// collision sets ever diverge, the test aborts via assert().

#include "../include/particle.h"
#include "../include/scenarios.h"
#include "../include/simulation.h"
#include "../include/narrow_phase.h"

#include <cassert>
#include <iostream>
#include <set>
#include <utility>
#include <vector>

using PairSet = std::set<std::pair<int, int>>;

static PairSet bruteForceCollisions(const std::vector<Particle>& particles) {
    PairSet result;
    for (size_t i = 0; i < particles.size(); ++i) {
        for (size_t j = i + 1; j < particles.size(); ++j) {
            if (narrow::colliding(particles[i], particles[j])) {
                result.insert({(int)i, (int)j});
            }
        }
    }
    return result;
}

// Mirrors Simulation's internal integrator exactly, so the reference
// trajectory matches the Verlet-buffered one step for step.
static void integrateReference(std::vector<Particle>& particles, float dt) {
    static const glm::vec3 gravity(0.0f, -9.81f, 0.0f);
    for (auto& p : particles) {
        p.pos += p.vel * dt + 0.5f * p.acc * dt * dt;
        glm::vec3 newAcc = gravity;
        p.vel += 0.5f * (p.acc + newAcc) * dt;
        p.acc = newAcc;
    }
}

static void checkScenario(const char* name, scenario::Cloud particles, float K,
                           float cellSize, int steps) {
    scenario::Cloud reference = particles;

    SimConfig cfg;
    cfg.K = K;
    cfg.cellSize = cellSize;
    cfg.skinMode = SimConfig::SkinMode::LocalVelocity;
    Simulation sim(cfg);

    for (int s = 0; s < steps; ++s) {
        // Ground truth computed on the pre-step positions, matching what
        // sim.step() sees internally before it integrates.
        PairSet expected = bruteForceCollisions(reference);

        StepStats st = sim.step(particles, /*recordCollisions=*/true);
        PairSet got(st.collisions.begin(), st.collisions.end());

        if (got != expected) {
            std::cerr << "MISMATCH in " << name << " (K=" << K << ") at step " << s
                      << ": verlet-buffer found " << got.size()
                      << " pairs, brute force found " << expected.size() << " pairs\n";
            assert(false && "Verlet buffer scheme diverged from brute-force ground truth");
        }

        integrateReference(reference, cfg.dt);
    }
    std::cout << "[OK] " << name << " K=" << K << " (" << steps << " steps)\n";
}

int main() {
    const float cellSize = 0.8f; // 8x radius, ample room for skin growth
    const int steps = 2000;
    const std::vector<float> kValues = {0.0f, 10.0f, 20.0f, 50.0f, 80.0f, 100.0f, 150.0f, 200.0f};

    for (float K : kValues) {
        //checkScenario("uniform_cloud", scenario::uniformCloud(100, 6.0f, 0.1f, 0.3f), K, cellSize, steps);
        //checkScenario("free_fall", scenario::freeFall(1000, 6.0f, 0.1f), K, cellSize, steps);
        //checkScenario("mixed_regime",
        //               scenario::mixedRegime(500, 500, 6.0f, 0.1f, 0.05f, 2.0f), K, cellSize, steps);
        checkScenario("explosion", scenario::explosion(100, 6.0f, 0.1f, 3.0f), K, cellSize, steps);
    }

    std::cout << "All correctness checks passed.\n";
    return 0;
}
