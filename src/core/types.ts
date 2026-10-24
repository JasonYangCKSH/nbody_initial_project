// =============================================================================
// types.ts：整個模擬共用的「資料長什麼樣子」
// -----------------------------------------------------------------------------
// TypeScript 的 type / interface 只描述資料的形狀，不會產生任何執行的程式。
// 先看懂這個檔案，其他檔案裡的變數名稱就都看得懂了。
// =============================================================================

// 三維向量：位置、速度、加速度都用它表示
export type Vec3 = { x: number; y: number; z: number };

// 一顆粒子身上所有的資料
export interface ParticleData {
  id: number;            // 粒子編號，等於它在陣列裡的位置（0, 1, 2, ...）
  position: Vec3;        // 目前位置
  velocity: Vec3;        // 目前速度（每秒移動多少）
  // 對應 C++ Particle::acc，一律存在（預設可為零向量）
  acceleration: Vec3;    // 加速度（每秒速度改變多少），煙火場景裡就是重力
  radius: number;        // 粒子半徑；兩顆粒子中心距離 ≤ 半徑和，就算碰撞
  // 對應 C++ Particle::mass（預設 1）
  mass: number;          // 質量，碰撞時用來算反彈
  // 「上一次重建候選列表（broad-phase）時」粒子所在的位置，又叫「快照」（論文 3.3 節）。
  // 現在位置和它的距離，就是論文式(2)的累積位移 Δx。
  positionAtLastBroadPhase: Vec3;
  // Verlet List 的緩衝距離 skin（論文式(1)）。只要粒子移動不超過 skin，
  // 舊的候選列表就還能用，不必重建。
  skin: number;
}

// 給畫面用的除錯幾何資訊（畫網格線、八元樹方框），和碰撞計算本身無關
export type DebugGeometry =
  | { kind: 'grid'; cellSize: number; bounds: Vec3 }
  // boxes：有粒子的葉節點；splits：已再往下切開的中間節點（含根節點，depth 0）
  | { kind: 'octree'; boxes: { center: Vec3; halfExtent: number; depth: number }[]; splits: { center: Vec3; halfExtent: number; depth: number }[] };

// 「空間結構」的共同介面：暴力法、均勻網格、八元樹都必須提供這些方法。
// 這樣主程式不需要知道現在用的是哪一種，呼叫方式都一樣（物件導向的「多型」）。
export interface SpatialStructure {
  // 把所有粒子放進結構裡（例如分進格子、插進樹）
  build(particles: ParticleData[]): void;
  // 找出「可能碰撞」的粒子配對，回傳 [粒子 a 的編號, 粒子 b 的編號] 的陣列。
  // withSkin = true 時，判斷距離會多加上兩顆粒子的 skin，找出「接下來幾步內可能碰撞」的配對。
  queryCandidatePairs(withSkin?: boolean): [number, number][];
  getDebugGeometry(): DebugGeometry;
  // 統計數字：這次做了幾次距離計算、找到幾組候選配對（畫面上的遙測數據）
  getMetrics(): { distanceChecks: number; candidatePairs: number };
  // 每顆粒子 skin 的上限（必須在 build() 之後呼叫）。
  // 均勻網格是 cellSize/2 − 半徑（論文式(3)），八元樹是 葉節點半邊長 − 半徑（論文式(4)）。
  // 後面的 ? 表示這個方法可以不提供（暴力法就沒有上限）。
  getSkinCaps?(particles: ParticleData[]): number[];
}

// 一次「重建事件」的紀錄，顯示在畫面的事件記錄裡
export interface RebuildEvent {
  step: number;                   // 第幾步發生
  triggeredByParticleId: number;  // 觸發重建的粒子（超出自己 skin 最多的那顆）
  displacement: number;           // 它的累積位移 Δx
  skinAtTrigger: number;          // 它當時的 skin（重建門檻）
}

// 每一步模擬結束後記錄的數據，用來畫遙測和耗時曲線
export interface StepMetrics {
  step: number;
  algorithm: string;
  elapsedMs: number;      // 這一步總共花了幾毫秒
  broadPhaseMs: number;   // 其中 broad-phase（找候選配對）花的時間；沒重建就是 0
  narrowPhaseMs: number;  // narrow-phase（逐一確認候選配對是否真的碰撞）花的時間
  distanceChecks: number;
  candidatePairs: number;
  collisions: number;     // 真的碰撞的配對數
  rebuilt: boolean;       // 這一步有沒有重建
  rebuildCount: number;   // 累計重建次數
  skippedSteps: number;   // 累計「沿用舊列表、跳過 broad-phase」的步數
}

export interface LogEntry extends RebuildEvent {
  message: string;
}

// 三個向量小工具
export const add = (a: Vec3, b: Vec3): Vec3 => ({ x: a.x + b.x, y: a.y + b.y, z: a.z + b.z });   // a + b
export const scale = (a: Vec3, factor: number): Vec3 => ({ x: a.x * factor, y: a.y * factor, z: a.z * factor }); // a × 倍數
// 兩點距離 = √(Δx² + Δy² + Δz²)；Math.hypot 就是在算這個
export const distance = (a: Vec3, b: Vec3): number => Math.hypot(a.x - b.x, a.y - b.y, a.z - b.z);
