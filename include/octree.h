// ***************************************************
// octree.h: 
// 此資料結構針對三維空間進行切割，並且
// 記錄各個切分的空間的質量(mass)與質心(center of mass)，
// 不存放body本身。
// ***************************************************
#ifndef OCTREE_H
#define OCTREE_H
#include <glm/glm.hpp>
#include <limits>
#include <array>
#include <algorithm>
#include "body.h"
// boundary class for octree node
// *****"class Oct" 即為boundary，起初沒有命名完善*****
class Oct {
public:
    glm::vec3 center;
    float size;

    // 1.new_containing():回傳該Node的boundary
    Oct new_containing(const std::vector<Body>& bodies) {
        float min_x = std::numeric_limits<float>::max();
        float min_y = std::numeric_limits<float>::max();
        float min_z = std::numeric_limits<float>::max();
        float max_x = std::numeric_limits<float>::lowest();
        float max_y = std::numeric_limits<float>::lowest();
        float max_z = std::numeric_limits<float>::lowest();

        for (Body body : bodies) {
            min_x = std::min(min_x, body.pos.x);
            min_y = std::min(min_y, body.pos.y);
            min_z = std::min(min_z, body.pos.z);
            max_x = std::max(max_x, body.pos.x);
            max_y = std::max(max_y, body.pos.y);
            max_z = std::max(max_z, body.pos.z);
        }
        
        center = glm::vec3(min_x + max_x, min_y + max_y, min_z + max_z) * 0.5f;
        size = std::max({max_x - min_x, max_y - min_y, max_z - min_z});
    
        return *this;
    
    } // end function

    // 2.findOctant():找到該pos(position)位於哪個index
    int findOctant(glm::vec3 pos) const {
        int index = 0;
        if (pos.x > center.x) index |= 1;
        if (pos.y > center.y) index |= 2;
        if (pos.z > center.z) index |= 4;
        
        return index;
    } // end function
    
    // 3.get_octant_boundary():取得該index的Oct資訊(center、size)
    Oct get_octant_boundary(int index) const {
        Oct sub;
        sub.size = size * 0.5f; 

        float offsetX = ((index & 1) ? 0.5f : -0.5f) * sub.size;
        float offsetY = ((index & 2) ? 0.5f : -0.5f) * sub.size;
        float offsetZ = ((index & 4) ? 0.5f : -0.5f) * sub.size;

        sub.center = center + glm::vec3(offsetX, offsetY, offsetZ);
        return sub;
    } // end function


    // 4.subdivide():基於當前的boundary進行劃分，回傳8個子boundary的Oct
    std::array<Oct, 8> subdivide() const {
        // 
        std::array<Oct, 8> children;
        for (int i = 0; i < 8; ++i)
            children[i] = get_octant_boundary(i);
        return children;
    } // end function
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
    bool isEmpty() const {return total_mass == 0;}


};
class Octree {
public:
    // ------使用平方來節省根號時間開銷------
    float t_sq;  // theta squared
    float e_sq;  // epsilon squared
    // ------------------------------------
    std::vector<Node> nodes; // Octree 資料結構本體，採取vector形式存放
    std::vector<int> parents; // index of the nodes with children 
                              // (額外做一個vector來存放parents' index，方便cache)

    static const int ROOT = 0;  // ROOT's index = 0

    Octree(float theta, float epsilon) {
        t_sq = theta * theta;
        e_sq = epsilon * epsilon;
    }
    // 1. clear(): reset or initialize the Octree
    void clear(Oct rootBoundary) {
        nodes.clear();
        parents.clear();
        nodes.emplace_back(0, rootBoundary);
    }

    // 2.subdivide(): 細分節點 (3D 版會產生 8 個孩子)，與Oct的subdivide()不同
    int subdivide(int node_idx) {
        parents.push_back(node_idx);  // 記錄有子節點的父節點索引
        int first_child_idx = nodes.size();  // new children will be added at the end of the nodes vector
        
        //藉由first_child來記錄其子節點位於vector的哪裡，故不須重新排列vector
        nodes[node_idx].first_child = first_child_idx;
        

        // 同時利用Oct的subdivide來劃分boundary
        std::array<Oct, 8> sub_boundaries = nodes[node_idx].boundary.subdivide();
        

        // 設定 8 個孩子的 next 指標
        // 前 7 個孩子指向下一個sibling，最後一個child指向父節點的下一個兄弟
        for (int i = 0; i < 8; ++i) {
            int next_val = (i < 7) ? (first_child_idx + i + 1) : nodes[node_idx].next_sibling;
            nodes.emplace_back(next_val, sub_boundaries[i]);
        }

        return first_child_idx;
    }

    // 3.insert():插入body
    void insert(glm::vec3 pos, float mass) {
        int node_idx = ROOT;

        // 1. 如果是分支，往下鑽
        while (nodes[node_idx].isBranch()) {
            int octant = nodes[node_idx].boundary.findOctant(pos);
            node_idx = nodes[node_idx].first_child + octant;
        }
        

        // 2. 如果是空葉子，直接放入
        if (nodes[node_idx].isEmpty()) {
            nodes[node_idx].com_pos = pos;
            nodes[node_idx].total_mass = mass;
            return;
        }

        // 3. 如果已有粒子且位置相同，累加質量 (避免無限細分)
        glm::vec3 p = nodes[node_idx].com_pos;
        float m = nodes[node_idx].total_mass;
        if (pos == p) {
            nodes[node_idx].total_mass += mass;
            return;
        }

        // 4. 衝突衝突！開始細分直到兩個粒子分開
        while (true) {
            
            int children_idx = subdivide(node_idx);
            

            int q1 = nodes[node_idx].boundary.findOctant(p);
            int q2 = nodes[node_idx].boundary.findOctant(pos);

            // 如果它們還是在同一個小格子裡，繼續切！
            if (q1 == q2) {
                node_idx = children_idx + q1;
            }
            // 終於分開了！分別放入對應的小格子 
            else {
                nodes[children_idx + q1].com_pos = p;
                nodes[children_idx + q1].total_mass = m;
                nodes[children_idx + q2].com_pos = pos;
                nodes[children_idx + q2].total_mass = mass;
                return;
            }
        }
    }

    // 4.propagate():從葉子向上傳遞，計算各節點的重心與總質量
    void propagate() {
        // 從最後建立的父節點開始反向遍歷 (即從底向上)
        for (auto it = parents.rbegin(); it != parents.rend(); ++it) {
            int n_idx = *it;
            int child_start = nodes[n_idx].first_child;
            
            glm::vec3 weighted_pos(0.0f);
            float m_sum = 0.0f;
            
            // 8 children 
            for (int i = 0; i < 8; ++i) {
                float m = nodes[child_start + i].total_mass;
                weighted_pos += nodes[child_start + i].com_pos * m;
                m_sum += m;
            }

            nodes[n_idx].total_mass = m_sum;
            if (m_sum > 0) nodes[n_idx].com_pos = weighted_pos / m_sum;
        }
    }

    // 計算給定位置受到的加速度 (Barnes-Hut 核心遍歷)
    glm::vec3 calculate_acc(glm::vec3 target_pos) const {
        glm::vec3 acceleration(0.0f);
        int node_idx = ROOT;

        while (true) {
            const Node& n = nodes[node_idx];
            glm::vec3 d = n.com_pos - target_pos;
            float d_sq = glm::dot(d, d);

            // Barnes-Hut 判斷公式: s / d < theta
            if (n.isLeaf() || (n.boundary.size * n.boundary.size < d_sq * t_sq)) {
                if (d_sq > 0) {
                    float dist = std::sqrt(d_sq);
                    // 引力公式包含 epsilon 防止除以 0 (Softening)
                    float denom = (d_sq + e_sq) * dist;
                    acceleration += d * (n.total_mass / denom);
                }
                
                if (n.next_sibling == 0) break; // 遍歷結束
                node_idx = n.next_sibling;
            } else {
                // 距離太近，進入子節點（更細的分法）
                node_idx = n.first_child;
            }
        }
        return acceleration;
    }

};
#endif