#pragma once
#include "Barnes-HutOctree.hpp"  // only referring Oct(boundary)
#include <vector>

class NeighborSearchNode {
private:
    Oct boundary;  // use Oct to represent AABB
    int firstChild;
    int nextSibling;
    std::vector<NeighborSearchNode> neighborNodesVector;
public:
    NeighborSearchNode(): boundary(), firstChild(0), nextSibling(0){}
    bool isLeaf() const {return (firstChild == 0);}
};



class NeighborSearchOctree {
private:
    std::vector<NeighborSearchNode> NSnodes;
    std::vector<int> NSparents;
    static const int ROOT = 0;
public:




};