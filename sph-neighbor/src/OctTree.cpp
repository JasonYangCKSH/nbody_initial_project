#include "OctTree.h"
#include <Eigen/Dense>
#include <iostream>
OctTree::OctTree() {
    root = new OctTreeNode();
   

}
OctTree::~OctTree() {

}
OctTreeNode* OctTree::getRoot() {return root;}
void OctTree::Reset(OctTreeNode *node) {
    // base case
    if (node == nullptr) return;
    for (int i = 0; i < 8; i++) {
        if (node->children[i] != nullptr) {
            Reset(node->children[i]);
            node = nullptr;
        }
    }
    delete node;
    
}
void OctTree::insert(std::vector<Particle> particles) {


}
void OctTree::findNeighbor(const std::vector<Particle> &particles,
                    std::vector<std::vector<int>> &neighbors, double search_radius) const {



}