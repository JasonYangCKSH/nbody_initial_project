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
};