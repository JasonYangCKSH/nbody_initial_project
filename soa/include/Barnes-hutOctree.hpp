#pragma once
#include <glm/glm.hpp>
#include <limits>
#include <array>
#include <algorithm>
#include "body_system.hpp"
class Oct{
public:
    glm::vec3 center;
    float size;
    Oct new_containing(const BodySystem& bs) {
        size_t n = bs.size();
        if (n == 0) return *this;

        float min_x = bs.posX[0], max_x = bs.posX[0];
        float min_y = bs.posY[0], max_y = bs.posY[0];
        float min_z = bs.posZ[0], max_z = bs.posZ[0];

        // 單次掃描所有維度，最大化快取利用率
        for (size_t i = 1; i < n; ++i) {
            if (bs.posX[i] < min_x) min_x = bs.posX[i];
            else if (bs.posX[i] > max_x) max_x = bs.posX[i];

            if (bs.posY[i] < min_y) min_y = bs.posY[i];
            else if (bs.posY[i] > max_y) max_y = bs.posY[i];

            if (bs.posZ[i] < min_z) min_z = bs.posZ[i];
            else if (bs.posZ[i] > max_z) max_z = bs.posZ[i];
        }

        center = glm::vec3(min_x + max_x, min_y + max_y, min_z + max_z) * 0.5f;
        size = std::max({max_x - min_x, max_y - min_y, max_z - min_z});

        return *this;
    }

    inline int findOctant(const BodySystem& bs, size_t i) const {
        // version 1
        /*int index = 0;
        if (bs.posX[i] > center.x) index |= 1;
        if (bs.posY[i] > center.y) index |= 2;
        if (bs.posZ[i] > center.z) index |= 4;
        return index;*/
        // version 2
        return (bs.posX[i] > center.x) | 
               ((bs.posY[i] > center.y) << 1) | 
               ((bs.posZ[i] > center.z) << 2);
    }
    Oct get_octant_boundary(int index) const {
        Oct sub;
        sub.size = size * 0.5f; 

        float offsetX = ((index & 1) ? 0.5f : -0.5f) * sub.size;
        float offsetY = ((index & 2) ? 0.5f : -0.5f) * sub.size;
        float offsetZ = ((index & 4) ? 0.5f : -0.5f) * sub.size;

        sub.center = center + glm::vec3(offsetX, offsetY, offsetZ);
        return sub;
    }

    std::array<Oct, 8> subdivide() const {
        // 
        std::array<Oct, 8> children;
        for (int i = 0; i < 8; ++i)
            children[i] = get_octant_boundary(i);
        return children;
    } 
};


class Node {
public:
    // --------------這些都是index-----------------
    int first_child;  // first child node's index
    int next_sibling;  // next sibling
    // -------------------------------------------

    glm::vec3 com_pos;  // position of "center of mass"
    float total_mass;  // total mass of all the particles in the node
    Oct boundary;

    // Constructor
    Node(int next_index, Oct b) :
        first_child(0), 
        next_sibling(next_index),
        com_pos(0.0f), 
        total_mass(0.0f),
        boundary(b) {}

    bool isLeaf() const {return first_child == 0;}
    bool isBranch() const {return first_child != 0;}
    bool isEmpty() const {return total_mass < 1e-9f;}
};

class Octree{
public:
    // ------使用平方來節省根號時間開銷------
    float t_sq;  // theta squared
    float e_sq;  // epsilon squared
    // ------------------------------------


    // -----NODE-------------------------------
    std::vector<int> first_child;
    std::vector<int> next_sibling;
    std::vector<float> com_posX, com_posY, com_posZ;
    std::vector<float> total_mass;
    std::vector<float> centerX, centerY, centerZ, node_size;

};