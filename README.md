## Start at 2026 January
### 基本規範
* 每週固定時段舉行小組討論。時段：14:00-15:00
> 小組討論必須全員到齊...
* 每週在此倉庫紀錄專題進度。
* 可事先規劃每週的預期項目。
Google Meet連結:
  https://meet.google.com/ubr-ifti-mfi
### 專題主題：改進BH-Octree演算法及模擬展示
* 建立這棵樹後就固定存在、物件移動時就reinsert（或 loose 容忍）、只更新受影響路徑摘要
### 專題方向
1. 無人機群（UAV Swarm，3D 空域）
- 更新需求（局部更新）
-- 觸發條件：單台 UAV 位置/高度變動，跨越 leaf 邊界（或超出 loose cell 容忍範圍)、UAV 狀態改變：進入/離開編隊、任務模式切換、通信品質變化
-- 更新物件（每台 UAV）：id, pos(xyz), vel, heading, radius(安全距離), class/role, timestamp
-- 節點摘要（沿著舊 leaf→LCA→新 leaf 的路徑）：count（此區域 UAV 數量）、COM/centroid（群集中心）、avg_vel 或 vel_sum（群集平均速度/動量近似）、max_radius（用於碰撞/近鄰剪枝）
-- 範例：UAV #17 從高度 30m 升到 45m，位置跨出原leaf：
--- 從原 leaf 的 object list 移除 #17
--- 在新 leaf 插入 #17
--- 只沿兩邊路徑更新 count/COM/vel_sum/max_radius（不重建整棵樹）
--- 若新 leaf 物件數超過容量才 split；原 leaf 過空才 merge（都屬局部結構調整）
- 查詢需求（即時查詢）
-- 局部避碰/鄰居查詢：找距離 r 內的 UAV（或最近 k 台）
-- 空域擁擠度：某航道/高度層是否過密
-- 編隊控制近似互動：對每台 UAV 計算「遠處群集的影響」（吸引/排斥/隊形保持）──這非常像 BH 的力計算
-- BH-Octree功能
--- 近鄰/避碰：走訪時若某 cell 的 bounding cube 與 UAV 的查詢球不相交直接剪枝；cell 太大/太近就下探
--- 群集影響：遠處 cell 直接用 COM + count + avg_vel 當成「一個群」近似（θ 控制精度/速度）
-- 範例：每 20ms 對每台 UAV 做一次：
--- r=10m 半徑內鄰居列表（嚴格）
--- 10m 以外用 BH 接受準則把遠處 cell 當群集，估算「整體排斥勢」避免往密集區鑽
--- 模擬展示：操作調整θ，看避碰安全性 vs 計算量的 trade-off
2. 賣場人潮（3D：含樓層/扶梯/高度，但本質是「3D 位置的移動體」）
- 更新需求（局部更新）
-- 觸發條件：來客每秒位置更新（手機藍牙 beacon、UWB、視覺追蹤），跨 leaf、上下樓/搭扶梯造成 z 變化明顯、顧客狀態改變：停留、排隊、群聚、快速移動（可能是異常）
-- 更新物件（每位顧客）：id, pos(xyz), vel, timestamp, group_id(可選), type(推車/輪椅…), weight(影響力)
-- 節點摘要（用於即時熱度/擁擠）：count（人數）、time-decayed count（熱度：越新的越重）、avg_speed（慢速聚集 vs 快速通行）、
-- 範例：某一區突然活動促銷，人群湧入：
--- 大量人進入同一群 leaf → 局部 split
--- 相鄰區變空 → 局部 merge
--- 全程不必重建整棵樹，但可設定「若同一層樓 occupancy 失衡太久」才觸發重建
-- 查詢需求（即時查詢）
--- 擁擠/排隊偵測：查某區域內人數是否 > 閾值（region query）
--- 動線建議/分流：找出「附近的替代路徑」或「低密度區域」
--- 保全巡檢：查詢「異常聚集」或「某店門口突然密度上升」
-- BH-Octree功能
--- 對「熱度/密度查詢」：遠處 cell 直接用 time-decayed count 回答，不用下探到人
--- 對「找最近的人/群」：近處下探，遠處用群集摘要估計「哪邊更擠」做快速決策
-- 範例：每秒更新一次「全館熱度圖」：對每個網格採樣點q：
--- 近處：下探到葉節點加總人（高精度）
--- 遠處：用 cell 的 time-decayed count 近似貢獻（BH acceptance）
--- 模擬展示誤差（與全精確加總相比）vs 訪問節點數。
3. 機器人群（AMR/倉儲機器人，3D 倉庫空間）
- 更新需求（局部更新）
-- 觸發條件：AMR 移動、轉彎、進出貨架通道（位置頻繁更新）、任務狀態切換：載貨/空車、避障模式、停靠充電、障礙物（叉車/臨時堆物）出現或消失（也算 3D 物件）
-- 更新物件（每台 AMR/障礙物）：AMR：id, pos(xyz), vel, heading, footprint(AABB/圓), priority, task_state、障礙物：id, bounds(AABB), static/dynamic, timestamp
-- 節點摘要：count（區域內機器人數）、max_footprint（剪枝/安全距離）
-- 範例：某通道出現臨時障礙物（叉車停放）：
--- 插入障礙物物件到對應 leaf（或 internal）
--- 沿路更新節點的 occupied flag / count
--- 只有該區域的 split/merge 可能改 reinforce，其他區域不動
- 查詢需求（即時查詢）
-- 路徑規劃的碰撞候選：給定 AMR 的 swept volume（未來 2 秒的膨脹盒），查有哪些障礙或機器人可能衝突
-- 近鄰協調：附近有哪些 AMR，誰該讓路（kNN/radius）
-- 區域壅塞偵測：某通道內機器人密度過高
-- BH-Octree功能
--- 碰撞候選：用 bounding cube 剪枝，命中區域才下探（典型 broad-phase）
--- 壅塞/流向：遠處 cell 用 count/avg_vel 直接回，快速找「哪條通道更塞」
-- 範例：每 100ms 對每台 AMR：
--- 查「未來 2 秒 swept AABB」相交的物件候選（只回候選，不做精確碰撞）
--- 查半徑 5m 內最近 5 台 AMR 來做讓路協調
--- 模擬展示候選對、查詢耗時、以及 loose factor 對更新成本的影響。
4. 星系（N-body gravitational system，最正宗 BH）
- 更新需求（局部更新）
-- 觸發條件：
--- 每個 timestep 所有天體位置都變（看起來「很多」），但你仍希望避免「每步整棵重建」
--- 星體在局部區域高密度聚集（星團核心），其他地方稀疏
-- 更新物件（每個星體）：id, mass, pos(xyz), vel, softening(可選), type(star/gas/dm), timestamp
-- 節點摘要（BH 力計算必要）：total_mass、center_of_mass（質量中心）、bounding cube
-- 範例
--- 若你採「固定深度的 static-topology octree」：結構不變，只更新每個 leaf 內的星體分配與節點摘要（仍是局部路徑更新）
--- 若你採「可分裂/合併的動態樹」：多數星體每步移動很小 → 很多仍留在原 leaf 或鄰近 leaf，只對跨界的星體做 reinsert；星團核心可能局部 split 更深、外圍 merge 更淺，當整體分布漂移太大（例如星系合併）才需要重建
- 查詢需求（即時查詢）
-- 對每個星體計算引力加速度（核心）
-- 區域質量/密度查詢：某半徑內質量、密度剖面
-- 找近距離相互作用：近場需要更精細（甚至直接 pairwise）
-- BH-Octree用途：對每個星體走訪樹：
--- 遠處 cell 用 total_mass + COM (+ multipole) 近似
--- 近處下探到更細或葉節點
--- θ 控制成本與誤差
-- 範例：每 timestep：
--- 對所有星體做一次 BH traversal 得到加速度
--- 同時提供「互動式查詢」：點選某星體即時計算 r=1pc 內質量與最近鄰列表
--- 模擬展示θ vs 能量漂移、θ vs 計算時間、以及局部重建觸發頻率。
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

>1/21-1/27
