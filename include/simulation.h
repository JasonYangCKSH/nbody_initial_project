#ifndef SIMULATION_H
#define SIMULATION_H
#include <vector>
#include <glm/glm.hpp>
#include "octree.h" 
class Simulation {
public:
    float dt;  // time step
    int frame;  // current frame number
    std::vector<Body> bodies;  // all bodies in the simulation
    Octree octree;  // Barnes-Hut octree for efficient force calculation
    Simulation(float _dt, float theta, float epsilon) :
        dt(_dt), frame(0), octree(theta, epsilon) {}
    void step() {
        this->iterate();  // update positions and velocities
        this->collide(); // handle collisions (find neighbor)
        this->attract(); // calculate gravitational forces and update accelerations
        frame++;
    }
    
private:
    // Kinematics "Update"
    void iterate() {
        // 如果粒子數目巨大，可能造成效能瓶頸
        for (Body& body : bodies) 
            body.update(dt);
    }
    // Barnes-Hut Logic
    void attract() {
        // 1.set up octree boundary
        Oct boundary = Oct().new_containing(bodies);

        // 2.clear and rebuild octree ==> BOTTLENECK
        // 每次都要重建一棵樹，造成效能瓶頸
        octree.clear(boundary);
        for (Body& body : bodies)
            octree.insert(body.pos, body.mass);

        // 3.propagate mass and center of mass up the tree
        octree.propagate();

        // 4.calculate acceleration for each body
        for (Body& body : bodies)
            body.acc = octree.calculate_acc(body.pos);
        
    }
    // Broad-phase collision detection and narrow-phase resolutionsss
    void collide() {
        // temporarily use O(n^2) to find the neighbor body
        for (int i = 0;  i < (int)bodies.size(); i++) {
            for (int j = i + 1; j < (int)bodies.size(); j++) {
                
                float dist_x = std::abs(bodies[i].pos.x - bodies[j].pos.x);
                float dist_y = std::abs(bodies[i].pos.y - bodies[j].pos.y);
                float dist_z = std::abs(bodies[i].pos.z - bodies[j].pos.z);
                float combinedRadius = bodies[i].radius + bodies[j].radius;

                // 只有當三個軸向的距離都小於半徑和，才進入精確的 resolve 計算
                if (dist_x < combinedRadius && dist_y < combinedRadius && dist_z < combinedRadius) {
                    this->resolve(i, j);
                }
            }
        }
    }
    // Resolve collision between body i (index) and body j (index)
    void resolve(int i, int j) {
        
        Body &b1 = bodies[i];
        Body &b2 = bodies[j];

        glm::vec3 p1 = b1.pos;
        glm::vec3 p2 = b2.pos;
        float r1 = b1.radius;
        float r2 = b2.radius;

        glm::vec3 d = p2 - p1;
        float r = r1 + r2;
        float dist_sq = glm::dot(d, d);

        // 1. 快速排除檢查
        if (dist_sq > r * r) {
            return;
        }
        glm::vec3 v1 = b1.vel;
        glm::vec3 v2 = b2.vel;
        glm::vec3 v = v2 - v1; // 相對速度
        float d_dot_v = glm::dot(d, v);

        float m1 = b1.mass;
        float m2 = b2.mass;
        float total_m = m1 + m2;
        float weight1 = m2 / total_m;
        float weight2 = m1 / total_m;

        // 2. 靜態重疊修正 (如果正在遠離但仍重疊)
        // d_dot_v >= 0 代表兩物體運動方向正在遠離
        if (d_dot_v >= 0.0f && dist_sq > 0.0f) {
            float dist = std::sqrt(dist_sq);
            glm::vec3 overlap_fix = d * (r / dist - 1.0f);
            bodies[i].pos -= weight1 * overlap_fix;
            bodies[j].pos += weight2 * overlap_fix;
            return;
        }

        // 3. 動態碰撞處理 (求解精確碰撞時間 t)
        float v_sq = glm::dot(v, v);
        float r_sq = r * r;
        
        // 如果相對速度太小，則不處理動態碰撞避免數值崩潰
        if (v_sq < 1e-8f) return;

        // 解二次方程判定碰撞時間 t
        float discriminant = d_dot_v * d_dot_v - v_sq * (dist_sq - r_sq);
        float t = (d_dot_v + std::sqrt(std::max(0.0f, discriminant))) / v_sq;

        // 回溯位置到碰撞瞬間
        bodies[i].pos -= v1 * t;
        bodies[j].pos -= v2 * t;

        // 更新碰撞瞬間的參數
        glm::vec3 current_p1 = bodies[i].pos;
        glm::vec3 current_p2 = bodies[j].pos;
        glm::vec3 current_d = current_p2 - current_p1;
        float current_d_sq = glm::dot(current_d, current_d);
        float current_d_dot_v = glm::dot(current_d, v);

        // 執行彈性碰撞 (1.5 是一個係數，可依需求調整彈性)
        if (current_d_sq > 0.0f) {
            glm::vec3 impulse = current_d * (1.5f * current_d_dot_v / current_d_sq);
            v1 = v1 + impulse * weight1;
            v2 = v2 - impulse * weight2;
        }

        // 更新速度並將時間前進回當前幀
        bodies[i].vel = v1;
        bodies[j].vel = v2;
        bodies[i].pos += v1 * t;
        bodies[j].pos += v2 * t;

    }
};
#endif