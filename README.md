## Start at 2026 January
### 基本規範

1/27(二)14:00-15:00 由於組員剛好有事，因此先暫時改動到1/27(二)上午10:00-11:00或1/28(三)上午10:00-11:00

* 每週固定時段舉行小組討論。時段：(二)14:00-15:00
> 小組討論必須全員到齊...
* 每週在此倉庫紀錄專題進度。
* 可事先規劃每週的預期項目。

Google Meet連結:
  https://meet.google.com/ubr-ifti-mfi
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

1. 經典奠基（必讀，了解 $O(N \log N)$ 的由來）Barnes, J., & Hut, P. (1986). "A hierarchical $O(N \log N)$ force-calculation algorithm." Nature.重點： 這是鼻祖論文，搞懂「接受準則 ($\theta$)」和節點摘要的定義。2. 動態更新與效能優化（專題核心參考）Dubinski, J. (1996). "A Parallel Tree Code." New Astronomy.重點： 雖然是講並行，但裡面討論了如何有效率地維護樹結構。Warren, M. S., & Salmon, J. K. (1993). "A parallel hashed octree N-body algorithm."重點： 討論了 Hash-based Octree，這對於「固定存在、物件移動」的索引優化很有啟發。3. 技術部落格與實作指南（快速上手用）"The Barnes-Hut Algorithm" by Tom Ventimiglia and Kevin Wayne (Princeton University).重點： 這份講義非常有系統地解釋了實作細節，是很多現成專案的理論來源。
---
---
YH: 
1. 看一下Issues老師的留言，要切換分支branch: main --> YH，裡面有二位要研究及討論的內容。
2. 蒐集及篩選論文是重要的研究能力，別仰賴Google就可以輕易取得的arXiv論文，那是未經同儕審查的預印本，大海撈針可能會徒勞無功。
---
---

JasonYang:
已看到老師的留言，謝謝老師。
