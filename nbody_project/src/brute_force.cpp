#include "nbody/brute_force.h"
#include <cmath>

void BruteForceCalculator::computeAccelerations(std::vector<Body>& bodies) {
    // 步驟1：把每個body的acceleration先歸零
    for (Body& b: bodies) {
        b.acceleration = Vec3(0.0, 0.0, 0.0);
    }
    // 步驟2：雙層迴圈，跑過每一對 (i, j)，注意 j 從 i+1 開始（為什麼？）
    for (size_t i = 0; i < bodies.size(); ++i) {
        for (size_t j = i + 1; j < bodies.size(); ++j) {

            // 步驟3：算出 r = bodies[j].position - bodies[i].position

            // 步驟4：算出 distSqr = r.dot(r) + softening*softening

            // 步驟5：算出 invDist3 = 1.0 / (distSqr * sqrt(distSqr))

            // 步驟6：更新 bodies[i].acceleration
            //         用公式：a_i += G * m_j * invDist3 * r
            //         （回想上一則訊息的公式4，分母質量是對方的m_j）

            // 步驟7：更新 bodies[j].acceleration
            //         方向相反，用牛頓第三定律：a_j += G * m_i * invDist3 * (-r)

        }
    }
}