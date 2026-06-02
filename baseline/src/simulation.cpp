#include "simulation.hpp"
#include <cmath>
#include <iostream>
#include <iomanip>
#include <glm/glm.hpp>
#include <vector>
#include <array>
#include <random>
// A
void Simulation::GenerateSimple() {
    bodies.clear();
    // pos                              vel         acc         mass  radius
    // --- 群組 A：互相都是鄰居 ---
    bodies.push_back(Body(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0), glm::vec3(0), 1.0f, 0.5f)); // 0

    
    bodies.push_back(Body(glm::vec3(0.5f, 0.0f, 0.0f), glm::vec3(0), glm::vec3(0), 1.0f, 0.5f)); // 1
    /*
    bodies.push_back(Body(glm::vec3(0.0f, 0.5f, 0.0f), glm::vec3(0), glm::vec3(0), 1.0f, 0.5f)); // 2

    // --- 群組 B：和 A 完全不相鄰 ---
    bodies.push_back(Body(glm::vec3(5.0f, 5.0f, 5.0f), glm::vec3(0), glm::vec3(0), 1.0f, 0.5f)); // 3
    bodies.push_back(Body(glm::vec3(5.5f, 5.0f, 5.0f), glm::vec3(0), glm::vec3(0), 1.0f, 0.5f)); // 4

    // --- 邊界案例：剛好在 searchRadius 上 ---
    bodies.push_back(Body(glm::vec3(0.0f, 0.999f, 0.0f), glm::vec3(0), glm::vec3(0), 1.0f, 0.5f)); // 5 ✓ 勉強在內
    bodies.push_back(Body(glm::vec3(0.0f, 1.001f, 0.0f), glm::vec3(0), glm::vec3(0), 1.0f, 0.5f)); // 6 ✗ 剛好在外

    // --- 負座標：測試 floor 是否正確 ---
    bodies.push_back(Body(glm::vec3(-0.3f, 0.0f, 0.0f), glm::vec3(0), glm::vec3(0), 1.0f, 0.5f)); // 7*/
}

void Simulation::GenerateRandom(int n, float rangeMin, float rangeMax) {
    bodies.clear();
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> posDist(rangeMin, rangeMax);
    std::uniform_real_distribution<float> massDist(0.5f, 2.0f);
    std::uniform_real_distribution<float> radiusDist(0.1f, 0.3f);


    for (int i = 0; i < n; i++) {
        glm::vec3 pos(posDist(rng), posDist(rng), posDist(rng));
        float mass   = massDist(rng);
        float radius = radiusDist(rng);
        bodies.push_back(Body(pos, glm::vec3(0), glm::vec3(0), mass, radius));
    }

}
// B

// 1. Brute Force
std::vector<NeighborPair> Simulation::BruteForce() {
    if (bodies.empty()) return {};
    
    std::vector<NeighborPair> pairs;
    float h2 = searchRadius * searchRadius;

    for (int i = 0; i < (int)bodies.size(); i++) { // traverse each body
        for (int j = i + 1; j < (int)bodies.size(); j++) { // j 初始化為 i+1，避免重複配對和自配對
            glm::vec3 d = bodies[j].pos - bodies[i].pos;
            float dist2 = d.x*d.x + d.y*d.y + d.z*d.z; // 計算內積(距離平方)
            if (dist2 > h2) continue; // 
            pairs.push_back({i, j, dist2});
        }
    }
    return pairs;
}

// 2. Uniform Grid
struct Cell {
    Cell (int posx, int posy, int posz): pos(posx, posy, posz){}
    glm::ivec3 pos;
};
int ComputeHashBucketIndex(Cell cell, int numBuckets) {
    const int h1 = 0x8da6b343;
    const int h2 = 0xd8163841;
    const int h3 = 0xcb1ab31f;
    int n = h1 * cell.pos.x + h2 * cell.pos.y + h3 * cell.pos.z;
    n = n % numBuckets;
    if (n < 0) n += numBuckets;
    return n;
}
std::vector<NeighborPair> Simulation::UniformGrid() {
    
    int numBuckets = std::max(1024, (int)bodies.size() * 2);
    std::vector<std::vector<int>> table(numBuckets);

    // build phase
    for (int i = 0; i < (int)bodies.size(); i++) {
        // transform particle position(x, y, z) to cell position(ix, iy, iz)
        int ix = (int)std::floor(bodies[i].pos.x / searchRadius);
        int iy = (int)std::floor(bodies[i].pos.y / searchRadius);
        int iz = (int)std::floor(bodies[i].pos.z / searchRadius);
        Cell cell(ix, iy, iz);
        // calculate hashbucket index and insert particle index into the bucket
        int bucket = ComputeHashBucketIndex(cell, numBuckets);
        table[bucket].push_back(i);
    }

    // query phase
    std::vector<NeighborPair> pairs;
    float h2 = searchRadius * searchRadius;

    for (int i = 0; i < (int)bodies.size(); i++) {
        int ix = (int)std::floor(bodies[i].pos.x / searchRadius);
        int iy = (int)std::floor(bodies[i].pos.y / searchRadius);
        int iz = (int)std::floor(bodies[i].pos.z / searchRadius);

        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                for (int dz = -1; dz <= 1; dz++) {
                    Cell neighbor(ix+dx, iy+dy, iz+dz);
                    int bucket = ComputeHashBucketIndex(neighbor, numBuckets);
                    for (int j : table[bucket]) {
                        if (j <= i) continue;
                        glm::vec3 d = bodies[j].pos - bodies[i].pos;
                        float dist2 = d.x*d.x + d.y*d.y + d.z*d.z;
                        if (dist2 > h2) continue;
                        pairs.push_back({i, j, dist2});
                    }
                }
            }
        }
    }
    return pairs;
}



// 3. Octree
struct OctreeNode {
    glm::vec3 center;      // 節點中心點
    float halfWidth;       // 半邊長
    OctreeNode* children[8]; // eight children
    std::vector<int> objects; // 存放 particle index

    OctreeNode(glm::vec3 c, float hw) : center(c), halfWidth(hw) {
        for (int i = 0; i < 8; i++) children[i] = nullptr;
    }
};
void InsertParticle(OctreeNode* node, int particleIdx, const std::vector<Body>& bodies, int depth, int maxDepth) {

    // end case: if reaching its threshold
    if (node->objects.size() < 8) {
        node->objects.push_back(particleIdx);
        return;
    }
    glm::vec3 pos = bodies[particleIdx].pos;
    glm::vec3 offset;
    float step = node->halfWidth * 0.5f;
    
    // set up the octant
    int index = 0;
    if (pos.x > node->center.x) index |= 1;
    if (pos.y > node->center.y) index |= 2;
    if (pos.z > node->center.z) index |= 4;

    if (node->children[index] == nullptr) {
        offset.x = (index & 1) ? step : -step;
        offset.y = (index & 2) ? step : -step;
        offset.z = (index & 4) ? step : -step;
        node->children[index] = new OctreeNode(node->center + offset, step);

    }
    InsertParticle(node->children[index], particleIdx, bodies, depth + 1, maxDepth);

}

OctreeNode* BuildOctree(const std::vector<Body>& bodies, 
                         float searchRadius) {
    if (bodies.empty()) return nullptr;

    // set minPos, maxPos of the body
    glm::vec3 minPos = bodies[0].pos;
    glm::vec3 maxPos = bodies[0].pos;
    for (const auto& b: bodies) {
        minPos = glm::min(minPos, b.pos);
        maxPos = glm::max(maxPos, b.pos);
    }

    // calculate: center position, halfWidth
    glm::vec3 center = (minPos + maxPos) * 0.5f;
    glm::vec3 diff = maxPos - minPos;
    float halfWidth = diff.x;
    if (halfWidth < diff.y) halfWidth = diff.y;
    if (halfWidth < diff.z) halfWidth = diff.z;
    halfWidth *= 0.5f;

    // add some padding 
    halfWidth += searchRadius;

    // determine the maxDepth
    // (halfWidth / searchRadius)
    int maxDepth = 0;
    float size = halfWidth;
    while (size > searchRadius && maxDepth < 8) {
        size *= 0.5f;
        maxDepth++;
    }

    OctreeNode *root = new OctreeNode(center, halfWidth);

    for (int i = 0; i < (int)bodies.size(); i++) {
        InsertParticle(root, i, bodies, 0, maxDepth);
    }
    return root;

}

void QueryNeighbors(OctreeNode* node, int queryIdx, const std::vector<Body>& bodies, float searchRadius, std::vector<NeighborPair>& pairs) {
    if (node == nullptr) return;
    glm::vec3 queryPos = bodies[queryIdx].pos;
    float dist2ToNode = 0.0f;
    // end case: -----------------------------------------
    for (int i = 0; i < 3; i++) {
        float v = queryPos[i];
        float min = node->center[i] - node->halfWidth;
        float max = node->center[i] + node->halfWidth;
        if (v < min) dist2ToNode += (min - v) * (min - v);
        if (v > max) dist2ToNode += (v - max) * (v - max);
    }
    if (dist2ToNode > searchRadius * searchRadius) return;
    // ---------------------------------------------------

    float h2 = searchRadius * searchRadius;
    for (int j : node->objects) {
        if (j <= queryIdx) continue; // 避免重覆
        glm::vec3 d = bodies[j].pos - queryPos;
        float dist2 = d.x*d.x + d.y*d.y + d.z*d.z;
        if (dist2 <= h2) {
            pairs.push_back({queryIdx, j, dist2});
        }
    }
    for (int i = 0; i < 8; i++) {
        QueryNeighbors(node->children[i], queryIdx, bodies, searchRadius, pairs);
    }

}
void DeleteOctree(OctreeNode* node) {
    if (node == nullptr) return;

    // 遞迴刪除所有子節點
    for (int i = 0; i < 8; i++) {
        DeleteOctree(node->children[i]);
    }

    delete node;
    node = nullptr;
}
std::vector<NeighborPair> Simulation::Octree() {
    if (bodies.empty()) return {};

    // 1. 建構 Octree
    OctreeNode* root = BuildOctree(bodies, searchRadius);

    // 2. 對每顆粒子查詢鄰居
    std::vector<NeighborPair> pairs;
    for (int i = 0; i < (int)bodies.size(); i++) {
        QueryNeighbors(root, i, bodies, searchRadius, pairs);
    }
    

    // 3. 釋放記憶體
    DeleteOctree(root);
    return pairs;

}
// C

void Simulation::PrintPairsResult(const std::vector<NeighborPair>& pairs) const {
    std::cout << std::left 
            << std::setw(15) << "INDEX_I" 
            << std::setw(15) << "INDEX_J" 
            << "DISTANCE" << "\n";
    std::cout << std::string(45, '-') << "\n";

    for (const auto& pair : pairs) {
        std::cout << std::left 
                << "INDEX: [" << std::setw(4) << pair.i << "], "
                << "[" << std::setw(4) << pair.j << "]; "
                << "DISTANCE: {" << std::fixed << std::setprecision(4) << std::sqrt(pair.distance2)<< "};\n";
    }
}