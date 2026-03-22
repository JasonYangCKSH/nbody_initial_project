#pragma once
#include <vector>
#include "body.hpp"

struct NSNode {

    // -both of these are used to define the boundary of a node-
    glm::vec3 aabb_min;  
    glm::vec3 aabb_max;
    // --------------------------------------------------------
    int firstChild;
    int nextSibling;
    std::vector<int> bodiesIndicesVector;
    static constexpr int MAX_LEAF_CAPACITY = 8;  

    NSNode():  firstChild(0), nextSibling(0){}
    bool isLeaf() const {return (firstChild == 0);}
    bool isEmpty() const {return bodiesIndicesVector.empty();}
    glm::vec3 GetAABBMin() const {return aabb_min;}
    glm::vec3 GetAABBMax() const {return aabb_max;}
    void SetAABBMin(glm::vec3 pos){aabb_min = pos;}
    void SetAABBMax(glm::vec3 pos){aabb_max = pos;}
    
};



class NSOctree {
private:
    std::vector<NSNode> nsNodes;
    std::vector<int> nsParents;
    std::vector<glm::vec3> positions;
    static const int ROOT = 0;
    glm::vec3 GetCenter(int nodeIdx) const {
        return (nsNodes[nodeIdx].GetAABBMin() + 
                nsNodes[nodeIdx].GetAABBMax()) * 0.5f;
    }

    // 判斷 pos 落在哪個 octant（0~7）
    int FindOctant(int nodeIdx, const glm::vec3& pos) const {
        glm::vec3 center = GetCenter(nodeIdx);
        int idx = 0;
        if (pos.x > center.x) idx |= 1;
        if (pos.y > center.y) idx |= 2;
        if (pos.z > center.z) idx |= 4;
        return idx;
    }
    void Insert(int nodeIdx, int bodyIdx) {
        // 情況 1：Branch Node → 往對應子節點走
        while (!nsNodes[nodeIdx].isLeaf()) {
            int octant = FindOctant(nodeIdx, positions[bodyIdx]);
            nodeIdx = nsNodes[nodeIdx].firstChild + octant;
        }

        // 情況 2：空 Leaf → 直接放入
        nsNodes[nodeIdx].bodiesIndicesVector.push_back(bodyIdx);

        // 情況 3：超過容量 → 分裂
        if ((int)nsNodes[nodeIdx].bodiesIndicesVector.size() 
                > NSNode::MAX_LEAF_CAPACITY) {
            Subdivide(nodeIdx);
        }
    }
    void Subdivide(int nodeIdx) {
        //  先把需要的資料全部取出來，再做 push_back
        glm::vec3 cMin = nsNodes[nodeIdx].aabb_min;
        glm::vec3 cMax = nsNodes[nodeIdx].aabb_max;
        int parentNextSibling = nsNodes[nodeIdx].nextSibling;
        
        nsParents.push_back(nodeIdx);
        int firstChildIdx = (int)nsNodes.size();
        nsNodes[nodeIdx].firstChild = firstChildIdx;  // 先設好

        glm::vec3 center = (cMin + cMax) * 0.5f;
        for (int i = 0; i < 8; i++) {
            NSNode child;
            child.aabb_min.x = (i & 1) ? center.x : cMin.x;
            child.aabb_min.y = (i & 2) ? center.y : cMin.y;
            child.aabb_min.z = (i & 4) ? center.z : cMin.z;
            child.aabb_max.x = (i & 1) ? cMax.x : center.x;
            child.aabb_max.y = (i & 2) ? cMax.y : center.y;
            child.aabb_max.z = (i & 4) ? cMax.z : center.z;
            child.nextSibling = (i < 7) ? (firstChildIdx + i + 1) : parentNextSibling;
            nsNodes.push_back(child);  // ✅ cMin/cMax 已經是 local copy，安全
        }

        // 重新分配粒子（注意：此時 nsNodes[nodeIdx] 仍需存取，但 push_back 已結束）
        std::vector<int> toRedistribute = nsNodes[nodeIdx].bodiesIndicesVector;
        nsNodes[nodeIdx].bodiesIndicesVector.clear();
        for (int bIdx : toRedistribute) {
            int octant = FindOctant(nodeIdx, positions[bIdx]);
            nsNodes[firstChildIdx + octant].bodiesIndicesVector.push_back(bIdx);
        }
    }
    void RangeQuery(int nodeIdx, const glm::vec3& pos, float r2, int queryIdx,
                    std::vector<int>& out) const {
        // 剪枝：搜尋球與此 Node 無交集
        if (!SphereOverlaps(nodeIdx, pos, r2)) return;

        if (nsNodes[nodeIdx].isLeaf()) {
            // Leaf：逐一精確檢查
            for (int idx : nsNodes[nodeIdx].bodiesIndicesVector) {
                if (idx == queryIdx) continue;
                glm::vec3 d = positions[idx] - pos;
                if (glm::dot(d, d) <= r2)
                    out.push_back(idx);
            }
        } else {
            // Branch：往子節點遞迴
            int childIdx = nsNodes[nodeIdx].firstChild;
            for (int i = 0; i < 8; i++)
                RangeQuery(childIdx + i, pos, r2, queryIdx, out);
        }
    }
    bool SphereOverlaps(int nodeIdx, const glm::vec3& pos, float r2) const {
        glm::vec3 aabbMin = nsNodes[nodeIdx].GetAABBMin();
        glm::vec3 aabbMax = nsNodes[nodeIdx].GetAABBMax();

        // 找 AABB 上距離 pos 最近的點
        glm::vec3 closest = glm::clamp(pos, aabbMin, aabbMax);
        glm::vec3 diff = pos - closest;
        return glm::dot(diff, diff) <= r2;
    }
public:

    void Build(const std::vector<Body>& bodies) {
        // step1: catch all position
        positions.clear();
        // step2: insert body's position step by step
        for (int i = 0; i < (int)bodies.size(); i++)
            positions.push_back(bodies[i].pos);
        nsNodes.clear();
        nsParents.clear();
        NSNode root;
        root.SetAABBMin(positions[0]);
        root.SetAABBMax(positions[0]);
        for (glm::vec3& pos: positions) {
            root.SetAABBMin(glm::min(root.GetAABBMin(), pos));
            root.SetAABBMax(glm::max(root.GetAABBMax(), pos));
        }
        nsNodes.push_back(root);
        // step3: insert every body step by step
        for (int i = 0; i < (int)positions.size(); i++) {
            this->Insert(ROOT, i);
        }
    }

    void Query(int idx, float r, std::vector<int>& out) const {
        out.clear();
        RangeQuery(ROOT, positions[idx], r * r, idx, out);
    }




};