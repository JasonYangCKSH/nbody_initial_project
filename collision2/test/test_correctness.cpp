// test_correctness.cpp
//
// 正確性驗證工具：把 UniformGrid / UniformGrid+skin / Octree / Octree+skin
// 這四種 broad-phase 設定跟 BruteForce（brute_force.h）逐一比對，確認：
//   1. candidate list 沒有漏掉任何真實碰撞（false negative，最嚴重的 bug）
//   2. candidate list 經過 narrow-phase 篩選後，恰好等於 BruteForce 的結果
//      （沒有多餘、也沒有短缺）
//
// 分兩個階段測試：
//   Part 1 - 靜態快照測試：針對各種手動構造的 edge case 與隨機場景，單一時間點
//            直接比對 Build() 出來的 candidate list。
//   Part 2 - 動態 skin 測試：模擬 Simulation::step() 的 rebuild 邏輯（build ->
//            紀錄 snapshot -> 更新 skin -> 移動 -> 檢查 listStillValid），驗證
//            在 skin 有效期間內，「沒有 rebuild 的舊 candidate list」仍然能涵蓋
//            粒子移動後的所有真實碰撞。這是 skin 機制真正要保證的不變量。

#include "particle.h"
#include "broad_phase.h"
#include "narrow_phase.h"
#include "brute_force.h"
#include "verlet_buffer.h"
#include "collision_response.h"
#include "scenario.h"

#include <functional>
#include <iostream>
#include <random>
#include <set>
#include <string>
#include <vector>

namespace {

using PairSet = std::set<std::pair<int, int>>;

PairSet toSet(const PairList& pairs) {
    PairSet s;
    for (auto [a, b] : pairs) {
        if (a > b) std::swap(a, b);
        s.emplace(a, b);
    }
    return s;
}

PairList toList(const PairSet& s) {
    PairList pairs;
    pairs.reserve(s.size());
    for (const auto& p : s) pairs.push_back(p);
    return pairs;
}

struct CheckResult {
    std::string label;
    bool ok = true;
    size_t candidateCount = 0;
    size_t truthCount = 0;
    size_t missingCount = 0;    // false negative：broad-phase 漏掉的真實碰撞（嚴重 bug）
    size_t extraCount = 0;      // narrow-phase 篩選後仍多出來的（理論上不該發生）
    size_t duplicateCount = 0;  // candidate list 本身重複的 pair 數
    std::vector<std::pair<int, int>> missingSample;
};

// 用 truth（brute force 的正確答案集合）驗證某個 broad-phase 方法產生的 candidates。
CheckResult checkAgainstTruth(const std::string& label, const PairList& candidates,
                               const std::vector<Particle>& particles, const PairSet& truth) {
    CheckResult r;
    r.label = label;
    r.candidateCount = candidates.size();
    r.truthCount = truth.size();

    PairSet candSet = toSet(candidates);
    r.duplicateCount = candidates.size() - candSet.size();

    PairSet filtered;
    for (const auto& [a, b] : candSet) {
        if (narrow::colliding(particles[a], particles[b])) filtered.emplace(a, b);
    }
    // count miss
    for (const auto& p : truth) {
        if (!candSet.count(p)) {
            r.missingCount++;
            if (r.missingSample.size() < 5) r.missingSample.push_back(p);
        }
    }
    // count extra
    for (const auto& p : filtered) {
        if (!truth.count(p)) r.extraCount++;
    }

    r.ok = (r.missingCount == 0) && (r.extraCount == 0);
    return r;
}

void printResult(const CheckResult& r) {
    std::cout << (r.ok ? "[PASS] " : "[FAIL] ") << r.label << " | candidates=" << r.candidateCount
              << " truth=" << r.truthCount << " missing=" << r.missingCount << " extra=" << r.extraCount
              << " dup=" << r.duplicateCount << "\n";
    if (!r.missingSample.empty()) {
        std::cout << "        missing sample: ";
        for (auto& [a, b] : r.missingSample) std::cout << "(" << a << "," << b << ") ";
        std::cout << "\n";
    }
}

// ---------------------------------------------------------------------------
// Part 1: 場景建構
// ---------------------------------------------------------------------------

struct Scene {
    std::string name;
    std::vector<Particle> particles;
    float worldSize;
    float cellSize;
};

Particle makeParticle(glm::vec3 pos, glm::vec3 vel, float radius) {
    Particle p;
    p.pos = pos;
    p.vel = vel;
    p.radius = radius;
    p.posAtLastBroadPhase = pos;
    return p;
}

Scene makeEdgeEmpty() { return {"edge_empty", {}, 20.0f, 2.0f}; }

Scene makeEdgeSingle() {
    return {"edge_single", {makeParticle({0, 0, 0}, {1, 0, 0}, 1.0f)}, 20.0f, 2.0f};
}

Scene makeOverlappingPair() {
    // radius 總和 = 2.0，距離 1.9 -> 有重疊，刻意避開跟 BruteForce 用 "<=" 而
    // narrow::colliding 用 "<" 判斷重疊的邊界不一致問題（見 brute_force.h / narrow_phase.h）
    std::vector<Particle> ps = {
        makeParticle({0.0f, 0, 0}, {0.5f, 0, 0}, 1.0f),
        makeParticle({1.9f, 0, 0}, {-0.5f, 0, 0}, 1.0f),
    };
    return {"pair_overlapping", ps, 20.0f, 2.0f};
}

Scene makeSeparatedPair() {
    std::vector<Particle> ps = {
        makeParticle({0, 0, 0}, {0, 0, 0}, 0.5f),
        makeParticle({10, 0, 0}, {0, 0, 0}, 0.5f),
    };
    return {"pair_separated", ps, 30.0f, 2.0f};
}

// 粒子刻意放在 cell 邊界兩側、彼此重疊，測試 UniformGrid 的 13-neighbor stencil
// 是否真的涵蓋每一種跨格方向（軸向 + 對角）。
Scene makeBoundaryStraddling(float cellSize) {
    const float eps = 0.1f;
    const float r = 0.3f;  // diameter(0.6) 遠小於 cellSize，符合 grid 的 cellSize 假設
    std::vector<glm::vec3> dirs = {
        {1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {1, 1, 0}, {1, 0, 1}, {0, 1, 1}, {1, 1, 1}, {1, -1, 1},
    };
    std::vector<Particle> ps;
    for (size_t i = 0; i < dirs.size(); ++i) {
        glm::vec3 n = glm::normalize(dirs[i]);
        glm::vec3 boundary = glm::vec3(cellSize) * static_cast<float>(2 * (i + 1));
        ps.push_back(makeParticle(boundary - n * eps, {0, 0, 0}, r));
        ps.push_back(makeParticle(boundary + n * eps, {0, 0, 0}, r));
    }
    return {"boundary_straddling", ps, cellSize * static_cast<float>(2 * (dirs.size() + 1)), cellSize};
}

Scene makeDenseCluster(int n, unsigned seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> posDist(0.0f, 5.0f);
    std::uniform_real_distribution<float> velDist(-1.0f, 1.0f);
    std::vector<Particle> ps(n);
    for (auto& p : ps) {
        p.pos = {posDist(rng), posDist(rng), posDist(rng)};
        p.vel = {velDist(rng), velDist(rng), velDist(rng)};
        p.radius = 0.6f;  // 半徑相對空間偏大 -> 大量重疊，考驗 candidate 完整性
        p.posAtLastBroadPhase = p.pos;
    }
    return {"dense_cluster_" + std::to_string(n), ps, 10.0f, 2.0f};
}

Scene makeVariedRadii(int n, float worldSize, unsigned seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> posDist(0.0f, worldSize);
    std::uniform_real_distribution<float> radiusDist(0.1f, 1.0f);
    std::uniform_real_distribution<float> velDist(-2.0f, 2.0f);
    std::vector<Particle> ps(n);
    for (auto& p : ps) {
        p.pos = {posDist(rng), posDist(rng), posDist(rng)};
        p.vel = {velDist(rng), velDist(rng), velDist(rng)};
        p.radius = radiusDist(rng);
        p.posAtLastBroadPhase = p.pos;
    }
    return {"varied_radii_" + std::to_string(n), ps, worldSize, 3.0f};
}

void runStaticSuite(int& totalChecks, int& failedChecks) {
    std::cout << "\n===== Part 1: 靜態快照測試 (single snapshot vs BruteForce) =====\n";

    std::vector<Scene> scenes;
    scenes.push_back(makeEdgeEmpty());
    scenes.push_back(makeEdgeSingle());
    scenes.push_back(makeOverlappingPair());
    scenes.push_back(makeSeparatedPair());
    scenes.push_back(makeBoundaryStraddling(2.0f));
    scenes.push_back(makeDenseCluster(150, 42));
    scenes.push_back(makeVariedRadii(300, 30.0f, 7));
    {
        Scene s;
        s.name = "scenario_uniformCloud_500";
        s.particles = scenario::uniformCloud(50000, 40.0f, 1.0f, 1.5f, 10.0f, 11);
        s.worldSize = 40.0f;
        s.cellSize = 3.0f;
        scenes.push_back(std::move(s));
    }
    {
        Scene s;
        s.name = "scenario_explosion_500";
        s.particles = scenario::explosion(500, 40.0f, 1.0f, 1.0f, 12);
        s.worldSize = 40.0f;
        s.cellSize = 3.0f;
        scenes.push_back(std::move(s));
    }

    for (auto& sc : scenes) {
        PairSet truth = toSet(BruteForce(sc.particles));

        // withSkin：用隨機到的速度算出一組非零 skin，讓 "+skin" 版本的行為跟
        // no-skin 版本真的有差異（見 broad_phase.h 裡 Octree 用 skin 擴張 margin）。
        std::vector<Particle> withSkin = sc.particles;
        verlet::updateLocalSkin(withSkin, /*K=*/2.0f, /*dt=*/1.0f / 60.0f);

        broad::UniformGrid grid(sc.cellSize);
        broad::Octree octree(/*maxDepth=*/10, /*leafCapacity=*/4, sc.worldSize);

        CheckResult r;
        r = checkAgainstTruth(sc.name + " | UniformGrid", grid.Build(sc.particles, false), sc.particles, truth);
        totalChecks++; failedChecks += !r.ok; printResult(r);

        r = checkAgainstTruth(sc.name + " | UniformGrid+Skin", grid.Build(withSkin, true), withSkin, truth);
        totalChecks++; failedChecks += !r.ok; printResult(r);

        r = checkAgainstTruth(sc.name + " | Octree", octree.Build(sc.particles, false), sc.particles, truth);
        totalChecks++; failedChecks += !r.ok; printResult(r);

        r = checkAgainstTruth(sc.name + " | Octree+Skin", octree.Build(withSkin, true), withSkin, truth);
        totalChecks++; failedChecks += !r.ok; printResult(r);
    }
}

// ---------------------------------------------------------------------------
// Part 2: 動態 skin 測試 —— 複製 Simulation::step() 的完整每幀節奏（build/reuse ->
// 比對 -> 彈性碰撞回應 -> integrate + 牆壁反彈），驗證「skin 有效期間內沿用舊
// candidate list」這個假設在粒子會反彈、速度會瞬間反轉的真實軌跡下仍然安全。
//
// 碰撞回應／牆壁反彈用的是 truth（BruteForce 算出的真實碰撞），而不是被測試中的
// cached candidate list，這樣物理軌跡本身不會受被測方法的 bug 影響，維持一份
// 乾淨、獨立於待測項目的參考軌跡。
// ---------------------------------------------------------------------------

using BuildFn = std::function<PairList(const std::vector<Particle>&, bool)>;

bool runDynamicSkinTest(const std::string& label, const BuildFn& build, std::vector<Particle> particles,
                         bool hasSkin, bool capSkinToCell, float K, float dt, float cellSize, float worldSize,
                         int frames, int& totalChecks, int& failedChecks) {
    bool allOk = true;
    PairList cached;
    bool needRebuild = true;
    int rebuildCount = 0;
    int failedFrames = 0;

    for (int f = 0; f < frames; ++f) {
        if (needRebuild) {
            cached = build(particles, hasSkin);
            verlet::recordBroadPhaseSnapshot(particles);
            if (hasSkin) {
                verlet::updateLocalSkin(particles, K, dt);
                if (capSkinToCell) verlet::capSkinToCellSize(particles, cellSize);
            }
            rebuildCount++;
        }

        // 用「目前」粒子位置算出真正的碰撞答案，跟沿用中的 cached candidate list 比對。
        PairSet truth = toSet(BruteForce(particles));
        CheckResult r = checkAgainstTruth(label + " frame " + std::to_string(f), cached, particles, truth);
        totalChecks++;
        if (!r.ok) {
            failedChecks++;
            failedFrames++;
            allOk = false;
            printResult(r);
        }

        // 跟 Simulation::step() 同樣的順序：先用真實碰撞做彈性碰撞回應，
        // 再 integrate（含牆壁反彈），驅動下一幀的粒子狀態。
        response::resolveCollisions(particles, toList(truth));
        for (auto& p : particles) {
            p.vel += p.acc * dt;
            p.pos += p.vel * dt;
        }
        response::reflectOffWalls(particles, worldSize);

        needRebuild = !verlet::listStillValid(particles);
    }

    std::cout << (allOk ? "[PASS] " : "[FAIL] ") << label << " | " << frames << " frames, rebuilds=" << rebuildCount
              << ", failed_frames=" << failedFrames << "\n";
    return allOk;
}

void runDynamicSuite(int& totalChecks, int& failedChecks) {
    std::cout << "\n===== Part 2: 動態 skin 測試 (Simulation::step 節奏下 vs BruteForce) =====\n";

    const float worldSize = 30.0f;
    const float cellSize = 3.0f;
    const float K = 0.0f;
    const float dt = 1.0f / 60.0f;
    const int frames = 40;

    struct DynamicCase {
        std::string namePrefix;
        std::vector<Particle> particles;
    };
    std::vector<DynamicCase> cases = {
        {"uniformCloud_1000", scenario::uniformCloud(1000, worldSize, 1.0f, 1.5f, 20000.0f, 21)},
        {"explosion_1000", scenario::explosion(1000, worldSize, 1.0f, 1.0f, 22)},
    };

    broad::UniformGrid grid(cellSize);
    broad::Octree octree(/*maxDepth=*/7, /*leafCapacity=*/40, worldSize);
    BuildFn gridBuild = [&](const std::vector<Particle>& p, bool skin) { return grid.Build(p, skin); };
    BuildFn octreeBuild = [&](const std::vector<Particle>& p, bool skin) { return octree.Build(p, skin); };

    for (auto& c : cases) {
        runDynamicSkinTest(c.namePrefix + " | UniformGrid no-skin", gridBuild, c.particles, /*hasSkin=*/false,
                            /*capSkinToCell=*/true, K, dt, cellSize, worldSize, frames, totalChecks, failedChecks);
        runDynamicSkinTest(c.namePrefix + " | UniformGrid+Skin", gridBuild, c.particles, /*hasSkin=*/true,
                            /*capSkinToCell=*/true, K, dt, cellSize, worldSize, frames, totalChecks, failedChecks);
        runDynamicSkinTest(c.namePrefix + " | Octree no-skin", octreeBuild, c.particles, /*hasSkin=*/false,
                            /*capSkinToCell=*/false, K, dt, cellSize, worldSize, frames, totalChecks, failedChecks);
        runDynamicSkinTest(c.namePrefix + " | Octree+Skin", octreeBuild, c.particles, /*hasSkin=*/true,
                            /*capSkinToCell=*/false, K, dt, cellSize, worldSize, frames, totalChecks, failedChecks);
    }
}

}  // namespace

int main() {
    int totalChecks = 0;
    int failedChecks = 0;

    runStaticSuite(totalChecks, failedChecks);
    runDynamicSuite(totalChecks, failedChecks);

    std::cout << "\n===== 總結 =====\n";
    std::cout << (failedChecks == 0 ? "[ALL PASS] " : "[SOME FAILED] ") << (totalChecks - failedChecks) << "/"
              << totalChecks << " checks passed\n";

    return failedChecks == 0 ? 0 : 1;
}
