#ifndef OCTREE_H
#define OCTREE_H
#include <vector>
#include <Eigen/Dense>
#include "Particle.h"
struct OctTreeNode {
    std::vector<Particle> particles;
    OctTreeNode *children[8] = {nullptr};  
    // 0:FTL 1:FTR 2:FBL 3:FBR
    // 4:BTL 5:BTR 6:BBL 7:BBR 
};

class OctTree {
private:
    OctTreeNode *root;
public:
    OctTree();
    ~OctTree();
    OctTreeNode *getRoot(); 
    void Reset(OctTreeNode *node);
    void findNeighbor(const std::vector<Particle> &particles,
                       std::vector<std::vector<int>> &neighbors, double search_radius) const;

    void insert(std::vector<Particle> particles);
    

    
};
#endif