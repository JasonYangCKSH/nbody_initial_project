#pragma once
#include <vector>
#include "body.hpp"

class NeighborSearchNode {
private:
    glm::vec3 aabb_min;
    glm::vec3 aabb_max;
    int firstChild;
    int nextSibling;
    std::vector<int> bodiesIndicesVector;
    static constexpr int MAX_LEAF_CAPACITY = 8;  
public:
    NeighborSearchNode():  firstChild(0), nextSibling(0){}
    bool isLeaf() const {return (firstChild == 0);}
    bool isEmpty() const {return bodiesIndicesVector.empty();}
};



class NeighborSearchOctree {
private:
    std::vector<NeighborSearchNode> NSnodes;
    std::vector<int> NSparents;
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

    }

    void Query(int idx, float r, std::vector<int>& out) const {

    }




};