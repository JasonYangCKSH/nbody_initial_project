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
        position.clear();
        for (Body& body : bodies) 
            position.push_vack(b.pos);
        // step2: build root node, aabb will wrap up all the particles
        nsNodes.clear();
        nsParent.clear();
        
    }

    void Query(int idx, float r, std::vector<int>& out) const {

    }




};