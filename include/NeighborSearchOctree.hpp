#pragma once
#include <vector>
#include "body.hpp"

struct NSNode {

    // -both of these are used to define the boundary of a node-
    glm::vec3 aabb_min;  
    glm::vec3 aabb_max;
    // --------------------------------------------------------
    int firstChild;  // to record the info of NSnode's child index
    int nextSibling; // to record its sibling's index
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
    std::vector<glm::vec3> positions;  // bodies' positions
    static const int ROOT = 0;
    // Find the center of this node based on its index
    glm::vec3 GetCenter(int nodeIdx) const {
        return (nsNodes[nodeIdx].GetAABBMin() + 
                nsNodes[nodeIdx].GetAABBMax()) * 0.5f;
    }

    // Determine which Octant the "pos" fall into
    int FindOctant(int nodeIdx, const glm::vec3& pos) const {
        glm::vec3 center = GetCenter(nodeIdx);
        int idx = 0;
        if (pos.x > center.x) idx |= 1;
        if (pos.y > center.y) idx |= 2;
        if (pos.z > center.z) idx |= 4;
        return idx;
    }
    void Insert(int nodeIdx, int bodyIdx) {
        // step1: isBranch
        while (!nsNodes[nodeIdx].isLeaf()) {
            int octant = FindOctant(nodeIdx, positions[bodyIdx]);
            nodeIdx = nsNodes[nodeIdx].firstChild + octant;
        }
        // step2: isLeaf
        nsNodes[nodeIdx].bodiesIndicesVector.push_back(bodyIdx);
        // step3: if nsNodes is over its capacity
        if (nsNodes[nodeIdx].bodiesIndicesVector.size() > NSNode::MAX_LEAF_CAPACITY) {
            Subdivide(nodeIdx);
        }
    }
    void Subdivide(int nodeIdx) {

        glm::vec3 cMin = nsNodes[nodeIdx].aabb_min;
        glm::vec3 cMax = nsNodes[nodeIdx].aabb_max;
        // load out parent next sibling first
        int parentNextSibling = nsNodes[nodeIdx].nextSibling;

        nsParents.push_back(nodeIdx);

        int firstChildIdx = (int)nsNodes.size();
        // origin: 0 after updated: firstChildIdx
        nsNodes[nodeIdx].firstChild = firstChildIdx;


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
            nsNodes.push_back(child);  
        }
        std::vector<int> tempBodiesIndiceVector = nsNodes[nodeIdx].bodiesIndicesVector;
        nsNodes[nodeIdx].bodiesIndicesVector.clear(); // since nsNode[nodeIdx] is no longer a leaf

        for (int idx: tempBodiesIndiceVector) {
            int octant = FindOctant(nodeIdx, positions[idx]);
            nsNodes[firstChildIdx + octant].bodiesIndicesVector.push_back(idx);
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
                RangeQuery(childIdx + i, pos, r2, queryIdx, out); // dfs
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
        // step1: reset bodies' position
        positions.clear();
        // step2: insert body's position step by step
        for (int i = 0; i < (int)bodies.size(); i++)
            positions.push_back(bodies[i].pos);


        // step3: reset nsNodes & nsParents(tree itself)
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
        // step4: insert every body step by step
        for (int i = 0; i < (int)positions.size(); i++) {
            this->Insert(ROOT, i);
        }
    }

    void Query(int idx, float r, std::vector<int>& out) const {
        out.clear();
        RangeQuery(ROOT, positions[idx], r * r, idx, out);
    }




};