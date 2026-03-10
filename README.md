# Baseline for barnes-hut algorithm (still under construction)
## Keywords:
1. AABB(Axis-aligned Bounding Box)、BVH（Bounding Volume Hierarchy）
2. Load Balancing
3. SIMD(AVX)
4. OPENMP/TBB ==> Parallel Computing
5. data mining: 判斷哪些Body set正在形成特殊天體
## Key points:
### [Barnes-hut octree] vs [AABB based BVH]
- Barnes-hut algorithm: 應用於**octree空間分割、計算body之間的重力吸引**，屬於**空間層級**。
- AABB(Axis-aligned Bounding Box): 應用於**天體(body的集合)**，用來處理如**兩個星系**碰撞之類的問題。
- BVH（Bounding Volume Hierarchy）: 將每個AABB存成樹狀結構，判斷在**極端環境(ex: 超大質量黑洞)**時是否能比Uniform Grid來的有效率。
'''

| Method     | Primary Purpose                  | Structure         |
| ---------- | -------------------------------- | ----------------- |
| Barnes–Hut | 優化計算重力的時間複雜度           | Octree            |
| AABB       | bounding volume representation   | box               |
| BVH        | collision detection acceleration | hierarchical tree |

'''
## Question:

1. 當前本專題的Octree專注於空間的分割，並套入barnes-hut演算法進行加速，然而在collision detection的部分，仍在使用$O(n^2)$，這部分的演算法我看原rust專案是採用call API的形式(broccoli)，這邊考慮採用c++實作，並設計三種比較的case(Brute force, BVH, uniform grid)。

2. data mining的部分插入點可能著重於星體的探索，找尋星體相關資料。

## 預期規劃：
**3/8 - 3/25:**
- 專注於AABB based BVH實作，並且測試效能(Brute force, BVH, uniform grid)
- 導入極端案例**質量巨大黑洞**。
## Reference:

- GitHub barnes-hut alrorithm: https://github.com/DeadlockCode/barnes-hut

- A Fast Parallel Processing Algorithm for Triangle Collision Detection Based on AABB and Octree Space Slicing in Unity3D **(2025)**: https://ieeexplore.ieee.org/abstract/document/10820353

- Load balancing n-body simulations with highly non-uniform density **(2014)**: https://dl.acm.org/doi/abs/10.1145/2597652.2597659

- SX-FlatTree: Pointerless Multi-Level Barnes-Hut N-Body Simulation via Z-Order Morton Encoding and Atomic Dispatch **(2026)**: https://www.researchgate.net/profile/Andres-Pirolo-2/publication/401427307_L_O_R_SX-FlatTree_Pointerless_Multi-Level_Barnes-Hut_N-Body_Simulation_via_Z-Order_Morton_Encoding_and_Atomic_Dispatch_201_speedup_over_ON_2_exact_at_055_force_error_Empirical_Pareto_frontier_for_N10_/links/69a55b01003023747db36175/L-O-R-SX-FlatTree-Pointerless-Multi-Level-Barnes-Hut-N-Body-Simulation-via-Z-Order-Morton-Encoding-and-Atomic-Dispatch-201-speedup-over-ON-2-exact-at-055-force-error-Empirical-Pareto-frontier-for-N10.pdf

- Numerical Performance Analysis of the Direct and Barnes-Hut Algorithm for Solving the N-Body Problem **(2025)**: https://link.springer.com/chapter/10.1007/978-3-032-08366-1_24#citeas

- A N-body Simulation using a
Bounding Volume Hierarchy
Structure **(2025)**: https://lup.lub.lu.se/luur/download?func=downloadFile&recordOId=9184695&fileOId=9184697

- N-body interactions and collisions in circumstellar discs for planar and inclined binary star configurations **(2025)** : https://academic.oup.com/mnras/article/545/4/staf2230/8383414?guestAccessKey=