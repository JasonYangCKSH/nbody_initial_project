#pragma once
#include <vector>
#include "body.hpp"

class NSNode {
private:
    // -both of these are used to define the boundary of a node-
    glm::vec3 aabb_min;  
    glm::vec3 aabb_max;
    // --------------------------------------------------------
    int firstChild;
    int nextSibling;
    std::vector<int> bodiesIndicesVector;
    static constexpr int MAX_LEAF_CAPACITY = 8;  
public:
    NSNode():  firstChild(0), nextSibling(0){}
    bool isLeaf() const {return (firstChild == 0);}
    bool isEmpty() const {return bodiesIndicesVector.empty();}
    glm::vec3 GetAABBMin() {return aabb_min;}
    glm::vec3 GetAABBMax() {return aabb_max;}
    void SetAABBMin(glm::vec3 pos){aabb_min = pos;}
    void SetAABBMax(glm::vec3 pos){aabb_max = pos;}
};



class NSOctree {
private:
    std::vector<NSNode> nsNodes;
    std::vector<int> nsParents;
    std::vector<glm::vec3> positions;
    static const int ROOT = 0;
    void Insert(int node_idx, int body_idx) {

    }
    void Subdivide(int node_idx) {

    }
    void RangeQuery(int node_idx, const glm::vec3& pos, float r2, int query_idx,
                    std::vector<int>& out) const {

    }
    void SphereOverlaps(int node_idx, const glm::vec3& pos, float r2) const {

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

    }




};