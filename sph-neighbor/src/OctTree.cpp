#include "OctTree.h"
#include <Eigen/Dense>
#include <iostream>
OctTree::OctTree(const Eigen::Vector3d &origin) {
    root = new OctTreeNode();
    root->particle = origin;
    std::cout << root->particle <<"\n";
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