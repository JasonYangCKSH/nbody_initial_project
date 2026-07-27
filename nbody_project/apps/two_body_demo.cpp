#include "nbody/body.h"
#include "nbody/brute_force.h"
#include "nbody/integrator.h"
#include <vector>
#include <iostream>
#include <cmath>

// TODO: 寫一個 totalEnergy(bodies, G) 函式
// KE = sum of 0.5 * mass * velocity.dot(velocity)
// PE = sum over i<j of  -G * m_i * m_j / distance(i,j)
// return KE + PE

int main() {
    const double G = 1.0;

    // TODO: 建立兩個Body放進vector
    // body0: mass大一點(例如10.0)，放在原點，靜止
    // body1: mass=1.0，放在(1,0,0)，給圓軌道速度

    // TODO: 算圓軌道速度 v_circ = sqrt(G * mass0 / r)
    // 提示：這個速度方向要垂直於position向量，才能形成圓軌道

    // TODO: 建立 BruteForceCalculator（softening先設0）
    // TODO: 建立 LeapfrogIntegrator，傳入calculator

    // TODO: 在開始迴圈前，先呼叫一次 calc.computeAccelerations(bodies)
    //       （為什麼這一步不能省略？回想Leapfrog的第一個kick需要什麼）

    // TODO: 算出理論週期 T = 2π * sqrt(r³ / (G * (m0+m1)))

    // TODO: 算出 E0 = totalEnergy(bodies, G)  ← 模擬前的能量

    // TODO: 決定 dt（提示：週期的1/1000左右），跑1000步的迴圈
    //       每一步呼叫 integrator.step(bodies, dt)

    // TODO: 算出 E1 = totalEnergy(bodies, G)  ← 模擬後的能量

    // TODO: 印出以下三樣東西：
    // 1. 能量相對誤差 |E1-E0|/|E0|  （應該很小，例如 < 1e-4）
    // 2. 理論週期數值
    // 3. body1最後的position（應該接近(1,0,0)，因為跑了整整一個週期）

    return 0;
}