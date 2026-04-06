#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <array>
#include <algorithm>
#include <cmath>
#include <limits>
#include "body_system.hpp"

// 僅作為臨時傳遞邊界資訊的輕量結構
struct Oct {
    glm::vec3 center;
    float size;
    Oct new_containing(const BodySystem& bs) {
        float min_x = std::numeric_limits<float>::max();
        float min_y = std::numeric_limits<float>::max();
        float min_z = std::numeric_limits<float>::max();
        float max_x = std::numeric_limits<float>::lowest();
        float max_y = std::numeric_limits<float>::lowest();
        float max_z = std::numeric_limits<float>::lowest();

        for (int i = 0; i < bs.posX.size(); i++) {
            min_x = std::min(min_x, bs.posX[i]);
            min_y = std::min(min_y, bs.posY[i]);
            min_z = std::min(min_z, bs.posZ[i]);
            max_x = std::max(max_x, bs.posX[i]);
            max_y = std::max(max_y, bs.posY[i]);
            max_z = std::max(max_z, bs.posZ[i]);
        }
        center = glm::vec3(min_x + max_x, min_y + max_y, min_z + max_z) * 0.5f;
        size = std::max({max_x - min_x, max_y - min_y, max_z - min_z});
        return *this;
    }
};

class Octree {
public:
    // --- 運算參數 ---
    float t_sq;  // theta squared
    float e_sq;  // epsilon squared

    // --- Node SoA 數據結構 (取代了原有的 vector<Node>) ---
    std::vector<int>   first_child;
    std::vector<int>   next_sibling;
    std::vector<float> comX, comY, comZ;
    std::vector<float> total_mass;
    std::vector<float> centerX, centerY, centerZ, node_size;

    // 用於向上傳遞質量的輔助索引
    std::vector<int> parents;

    static const int ROOT = 0;

    Octree(float theta, float epsilon) : 
        t_sq(theta * theta), e_sq(epsilon * epsilon) {}

    // 輔助函式：新增一個節點並返回其索引
    int add_node(int next_sib, float cx, float cy, float cz, float sz) {
        int idx = first_child.size();
        first_child.push_back(0);
        next_sibling.push_back(next_sib);
        comX.push_back(0.0f); comY.push_back(0.0f); comZ.push_back(0.0f);
        total_mass.push_back(0.0f);
        centerX.push_back(cx); centerY.push_back(cy); centerZ.push_back(cz);
        node_size.push_back(sz);
        return idx;
    }

    // 1. clear: 初始化 ROOT
    void clear(Oct root) {
        first_child.clear(); next_sibling.clear();
        comX.clear(); comY.clear(); comZ.clear();
        total_mass.clear();
        centerX.clear(); centerY.clear(); centerZ.clear();
        node_size.clear();
        parents.clear();

        add_node(0, root.center.x, root.center.y, root.center.z, root.size);
    }

    // 2. subdivide: 產生 8 個子節點
    // Atomic operation
    int subdivide(int node_idx) {
        parents.push_back(node_idx);
        int first_child_idx = first_child.size();
        first_child[node_idx] = first_child_idx;

        float sub_size = node_size[node_idx] * 0.5f;
        float cx = centerX[node_idx];
        float cy = centerY[node_idx];
        float cz = centerZ[node_idx];

        for (int i = 0; i < 8; ++i) {
            float ox = ((i & 1) ? 0.5f : -0.5f) * sub_size;
            float oy = ((i & 2) ? 0.5f : -0.5f) * sub_size;
            float oz = ((i & 4) ? 0.5f : -0.5f) * sub_size;
            
            int next_val = (i < 7) ? (first_child_idx + i + 1) : next_sibling[node_idx];
            add_node(next_val, cx + ox, cy + oy, cz + oz, sub_size);
        }
        return first_child_idx;
    }

    // 3. insert: 針對 BodySystem 的單個粒子插入
    void insert(const BodySystem& bs, size_t b_idx) {
        float px = bs.posX[b_idx], py = bs.posY[b_idx], pz = bs.posZ[b_idx];
        float m = bs.mass[b_idx];
        int node_idx = ROOT;

        // 往下找葉子
        while (first_child[node_idx] != ROOT) {
            int q = (px > centerX[node_idx]) | ((py > centerY[node_idx]) << 1) | ((pz > centerZ[node_idx]) << 2);
            node_idx = first_child[node_idx] + q;
        }

        // Case: 空葉子
        if (total_mass[node_idx] < 1e-9f) {
            comX[node_idx] = px; comY[node_idx] = py; comZ[node_idx] = pz;
            total_mass[node_idx] = m;
            return;
        }

        // Case: 已有粒子
        float ex = comX[node_idx], ey = comY[node_idx], ez = comZ[node_idx];
        float em = total_mass[node_idx];
        if (px == ex && py == ey && pz == ez) {
            total_mass[node_idx] += m;
            return;
        }

        // 衝突，細分
        while (true) {
            int child_start = subdivide(node_idx);
            int q_old = (ex > centerX[node_idx]) | ((ey > centerY[node_idx]) << 1) | ((ez > centerZ[node_idx]) << 2);
            int q_new = (px > centerX[node_idx]) | ((py > centerY[node_idx]) << 1) | ((pz > centerZ[node_idx]) << 2);

            if (q_old == q_new) {
                node_idx = child_start + q_old;
            } else {
                comX[child_start + q_old] = ex; comY[child_start + q_old] = ey; comZ[child_start + q_old] = ez;
                total_mass[child_start + q_old] = em;
                comX[child_start + q_new] = px; comY[child_start + q_new] = py; comZ[child_start + q_new] = pz;
                total_mass[child_start + q_new] = m;
                return;
            }
        }
    }

    // 4. propagate: 向上計算重心與總質量
    void propagate() {
        for (auto it = parents.rbegin(); it != parents.rend(); ++it) {
            int p_idx = *it;
            int c_start = first_child[p_idx];
            float m_sum = 0, wx = 0, wy = 0, wz = 0;

            for (int i = 0; i < 8; ++i) {
                float cm = total_mass[c_start + i];
                m_sum += cm;
                wx += comX[c_start + i] * cm;
                wy += comY[c_start + i] * cm;
                wz += comZ[c_start + i] * cm;
            }
            total_mass[p_idx] = m_sum;
            if (m_sum > 0) {
                comX[p_idx] = wx / m_sum; comY[p_idx] = wy / m_sum; comZ[p_idx] = wz / m_sum;
            }
        }
    }

    // 5. calculate_acc: 核心計算邏輯 (SoA 優勢所在)
    glm::vec3 calculate_acc(float tx, float ty, float tz) const {
        glm::vec3 acc(0.0f);
        int idx = ROOT;

        while (true) {
            float dx = comX[idx] - tx;
            float dy = comY[idx] - ty;
            float dz = comZ[idx] - tz;
            float d_sq = dx*dx + dy*dy + dz*dz;
            float sz = node_size[idx];

            if (first_child[idx] == 0 || (sz * sz < d_sq * t_sq)) {
                if (d_sq > 0 && total_mass[idx] > 0) {
                    float dist = std::sqrt(d_sq);
                    float denom = (d_sq + e_sq) * dist;
                    float factor = total_mass[idx] / denom;
                    acc.x += dx * factor;
                    acc.y += dy * factor;
                    acc.z += dz * factor;
                }
                if (next_sibling[idx] == 0) break;
                idx = next_sibling[idx];
            } else {
                idx = first_child[idx];
            }
        }
        return acc;
    }
};