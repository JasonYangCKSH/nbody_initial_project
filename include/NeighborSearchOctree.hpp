#pragma once
#include "Barnes-HutOctree.hpp"  // only referring Oct(boundary)
#include <vector>
class NeighborSearchNode {
private:
    Oct boundary;
    
public:

};



class NeighborSearchOctree {
private:
    std::vector<NeighborSearchNode> NSnodes;
    std::vector<int> NSparents;
    static const int ROOT = 0;
public:




};