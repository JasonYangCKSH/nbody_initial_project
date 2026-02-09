we## Start at 2026 January
### 基本規範

1/27(二)14:00-15:00 由於組員剛好有事，因此先暫時改動到1/28(三)上午10:00-12:00

* 每週固定時段舉行小組討論。時段：(二)14:00-15:00
> 小組討論必須全員到齊...
* 每週在此倉庫紀錄專題進度。
* 可事先規劃每週的預期項目。

Google Meet連結:
https://meet.google.com/kcz-wwxd-mxw

PPT連結：
https://www.canva.com/design/DAG-MFxhIL4/L786ypSan6urp5cNlIpC0A/edit
---
---
1/14-1/20

Matthias Müller SPH 2003(SPH論文，這篇第 4.2 節詳細講 Spatial Hashing):
https://matthias-research.github.io/pages/publications/sca03.pdf

Octree開山之作(1982):
dblp搜尋關鍵字:"Geometric modeling using octree encoding"

Octree教學：
https://fab.cba.mit.edu/classes/S62.12/docs/Meagher_octree.pdf

"ParallelNN: A Parallel Octree-based Nearest Neighbor Search Accelerator for 3D Point Clouds":
https://ieeexplore.ieee.org/document/10070940

Barnes-Hut Algorithm:
"A hierarchical O(N log N) force-calculation algorithm":
https://www.nature.com/articles/324446a0


> 平台能透過點擊能設定每個星球的 質量 密度 大小
> 改 m → 反算 ρ
> 改 r 或 ρ → 算 m

> 每一幀更新完速度後，乘上阻尼
damping = 1：完全不減速，越跑越快（引力加速度）
damping = 0.997：每一幀把速度變成原本的 99.7%
> 
>預期項目:了解如何把 2D→3D

1/21-1/27

本次暫時規劃：


1. 經典奠基理論

* **文獻名稱：** Barnes, J., & Hut, P. (1986). "A hierarchical O(N log N) force-calculation algorithm." Nature.
* **研究重點：** * 了解 Barnes-Hut 演算法的鼻祖理論。
* 深入研究「接受準則 (Multipole Acceptance Criterion, θ)」的數學定義。
* 理解節點摘要（Node Summary）如何簡化遠程力計算，將複雜度從 O(N^2) 降至 O(N log N)。


2. 動態更新與效能優化

* **文獻名稱：** Dubinski, J. (1996). "A Parallel Tree Code." New Astronomy.
* **研究重點：** * 雖然主軸為並行運算，但其對樹結構維護（Tree Maintenance）的效率討論極具參考價值。
* 學習如何在物件位移時，最小化樹的重建開銷。

* **文獻名稱：** Warren, M. S., & Salmon, J. K. (1993). "A parallel hashed octree N-body algorithm."
* **研究重點：** * 探討「Hash-based Octree」的實作方法。
* 對於「物件頻繁移動但樹結構相對固定」的索引優化具有啟發性，有助於實作局部更新邏輯。

3. 技術實作指南與教學資源

* **資源名稱：** "The Barnes-Hut Algorithm" by Tom Ventimiglia and Kevin Wayne (Princeton University).
* **研究重點：** * 普林斯頓大學的經典講義，系統性地解釋了演算法的實作細節。
* 作為開源專案實作邏輯的主要參考來源，有助於將理論轉化為程式碼。

4. 網站開源資源(包含github)

https://medium.com/@hsinhungw/optimizing-n-body-simulation-with-barnes-hut-algorithm-and-cuda-c76e78228c28

5. Hackmd統整重點區(目前已經先對內部三篇文章進行初步的知識吸收，暫時有一些初步的想法)：
https://hackmd.io/aaZ9QlPsTau3RrRKIMhVOw


1/28 - 2/2

1.針對Barnes-hut 演算法原理進行深度了解，理解其如何影響finding neighbor的時間複雜度。

2.閱讀https://dl.acm.org/doi/epdf/10.1145/3550454.3555523 論文，從中找到相關finding neighbor的方法，熟悉其原理。

3.稍微理解使用B-tree優化finding neighbor效率的arXiv論文 https://arxiv.org/abs/1910.02639 。

4.思考如何將Barnes-hut演算法運用於finding neighbor 以及研究方向。

2/3 - 2/9


2/9 - 2/15


---

使用base44 來協助製作3d版本的星球模擬器https://celestial-dynamics-simulator-b0221e1b.base44.app/

但是查看Base44 目前對 3D 顯示有明確限制：
官方回饋論壇提到「目前無法正確顯示 3D 物件」，雖然 Three.js 能畫出基本立方體，但讀取 3D 檔（obj / gltf 等）做不太起來。

所以如果你要做出真實的3d效果，還需要再使用其他的辦法，目前事件是能使用來做介紹教學，這些比較基礎的東西。把 Base44 當外層網站
