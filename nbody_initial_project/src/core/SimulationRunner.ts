// =============================================================================
// SimulationRunner：一份完整的模擬（粒子、空間結構、Verlet 控制器、候選列表與每步數據）
// -----------------------------------------------------------------------------
// 原本寫在 App.tsx 的 step() 搬到這裡，畫面只負責「每隔一段時間呼叫 step()、把結果畫出來」。
// 好處：同一個畫面可以同時跑兩份（劇本二：上均勻網格、下八元樹，同一組初始條件），
// 單元測試也可以直接跑整個流程，不需要瀏覽器。
// =============================================================================
import { BruteForceStructure } from './spatial/BruteForceStructure';
import { OctreeStructure } from './spatial/OctreeStructure';
import { UniformGridStructure } from './spatial/UniformGridStructure';
import { Instrumentation } from './metrics/Instrumentation';
import { ParticleSystem, type Scene } from './ParticleSystem';
import { VerletBufferController } from './VerletBufferController';
import type { RebuildEvent, SpatialStructure, StepMetrics, Vec3 } from './types';

export type Algorithm = 'Brute Force' | 'Uniform Grid' | 'Octree';

// 一份模擬的全部設定；一鍵套用劇本，就是換一組 SimConfig 再重新產生粒子
export interface SimConfig {
  scene: Scene;
  count: number;
  worldSize: number;      // 模擬空間是邊長 worldSize 的立方體
  radius: number;         // 粒子半徑（全部相同）
  speed: number;          // 粒子雲：每軸初速範圍 ±speed；小空間：焦點球的速率
  clusterFactor: number;  // 粒子雲的群聚係數（0 = 均勻分布）
  clusters: number;       // 粒子雲的群聚中心數，或煙火發數
  acceleration: number;   // 粒子雲：每軸加速度範圍 ±acc；煙火：重力；小空間不使用（等速）
  seed: number;           // 亂數種子：種子相同，初始條件就相同
  algorithm: Algorithm;
  bufferEnabled: boolean; // Verlet 緩衝：開啟後沿用候選列表，直到有粒子走出自己的 skin
  K: number;              // skin 預測幾步之後的位移（論文式(1)）
  leafCapacity: number;   // 八元樹葉節點容量：超過就切成 8 格
  maxDepth: number;       // 八元樹最大深度：到了就不再切
}

// 「上一幀」用的快照：跑一步之前的完整狀態。每顆粒子存 13 個數字（位置、速度、加速度、skin、上次重建時的位置）；
// 其他欄位（候選配對、碰撞配對、數據）每一步都是換成新的陣列或物件、不會原地修改，所以直接保存參照即可
interface Snapshot {
  particles: Float64Array;
  pairs: [number, number][];
  candidateIds: Set<number>;
  listVersion: number;
  collisionPairs: [number, number][];
  collisionIds: Set<number>;
  newCollisionPairs: [number, number][];
  metrics: StepMetrics;
  events: RebuildEvent[];
  triggerCounts: [number, number][];
  lastRebuild: SimulationRunner['lastRebuild'];
  previousCollisions: Set<number>;
  forceRebuild: boolean;
  K: number;
  history: StepMetrics[];
}
const NUMBERS_PER_PARTICLE = 13;

export const DT = 1 / 60;
// 均勻網格的格長 = cellSizeRatio(2) × 直徑，與 C++ bench 一致（半徑 0.075 → 0.3）。
// 格長必須跟著半徑變：格長小於兩倍直徑時，兩顆相碰的球可能隔兩格，只搜尋相鄰格就會漏抓
export const cellSizeFor = (radius: number) => 2 * 2 * radius;

// 第 0 步必定重建（見 C++ Simulation::needsRebuild），計數從 0 開始，由第一次 step 計入
export const initialMetrics = (algorithm: string): StepMetrics => ({ step: 0, algorithm, elapsedMs: 0, checkMs: 0, broadPhaseMs: 0, narrowPhaseMs: 0, detectionMs: 0, broadPhaseChecks: 0, distanceChecks: 0, candidatePairs: 0, collisions: 0, rebuilt: false, rebuildCount: 0, skippedSteps: 0 });

export class SimulationRunner {
  readonly config: SimConfig;
  readonly bounds: Vec3;
  readonly cellSize: number;
  readonly system: ParticleSystem;
  readonly structure: SpatialStructure;
  readonly controller: VerletBufferController;
  readonly instrumentation = new Instrumentation();
  // 目前沿用的候選配對（最近一次重建時找到的），以及出現在候選配對裡的粒子
  pairs: [number, number][] = [];
  candidateIds = new Set<number>();
  // 每次重建 +1；畫面用它判斷候選列表是否換過
  listVersion = 0;
  // 這一步真正碰撞的配對（已排序）與碰撞中的粒子；newCollisionPairs 是上一步還沒碰撞、這一步才碰撞的配對
  collisionPairs: [number, number][] = [];
  collisionIds = new Set<number>();
  newCollisionPairs: [number, number][] = [];
  metrics: StepMetrics;
  // 最近 12 次「有粒子超出 skin」觸發的重建，最新的在最前面
  events: RebuildEvent[] = [];
  // 最近一次「沿用名單後」的重建（第 1 步的初次建立不算），畫面用來顯示重建提示；triggerId 為 null 表示是調整 K 造成的
  lastRebuild: { step: number; rebuildCount: number; triggerId: number | null; detectionMs: number } | null = null;
  // 每顆粒子觸發重建的累計次數（events 只保留最近 12 筆，這裡保留全部）
  readonly triggerCounts = new Map<number, number>();
  private previousCollisions = new Set<number>();
  private forceRebuild = false;
  // 「上一幀」：最近幾步之前的快照（最新的在最後）；maxSnapshots 為 0 表示不保存
  private snapshots: Snapshot[] = [];
  private maxSnapshots = 0;

  constructor(config: SimConfig) {
    this.config = { ...config };
    const size = config.worldSize;
    this.bounds = { x: size, y: size, z: size };
    this.cellSize = cellSizeFor(config.radius);
    this.system = new ParticleSystem(config.count, this.bounds, config.clusterFactor, config.acceleration, config.seed, config.scene, config.clusters, { radius: config.radius, speed: config.speed });
    this.structure = config.algorithm === 'Uniform Grid' ? new UniformGridStructure(this.bounds, this.cellSize)
      : config.algorithm === 'Octree' ? new OctreeStructure(this.bounds, config.maxDepth, config.leafCapacity)
        : new BruteForceStructure();
    this.controller = new VerletBufferController(0.15, config.K, DT);
    this.metrics = initialMetrics(config.algorithm);
  }

  get particles() { return this.system.particles; }

  // 調整 K：下一步直接重建，新的 skin 馬上生效。
  // 不重建也不會漏抓（判定一律用重建當時的 skin），只是要等下次重建才看得到差別
  setK(K: number): void {
    this.config.K = K;
    this.controller.K = K;
    this.forceRebuild = true;
  }

  // 開啟「上一幀」：最多保存 frames 步之前的狀態（0 = 關閉並清掉已保存的）
  keepHistory(frames: number): void {
    this.maxSnapshots = frames;
    this.snapshots = frames > 0 ? this.snapshots.slice(-frames) : [];
  }

  get canStepBack(): boolean { return this.snapshots.length > 0; }

  // 回到上一步之前的狀態。之後再往前跑，結果和原本一模一樣（模擬每一步都是固定的計算，沒有亂數）
  stepBack(): boolean {
    const snapshot = this.snapshots.pop();
    if (!snapshot) return false;
    this.system.particles.forEach((particle, index) => {
      const v = snapshot.particles.subarray(index * NUMBERS_PER_PARTICLE, (index + 1) * NUMBERS_PER_PARTICLE);
      particle.position = { x: v[0], y: v[1], z: v[2] };
      particle.velocity = { x: v[3], y: v[4], z: v[5] };
      particle.acceleration = { x: v[6], y: v[7], z: v[8] };
      particle.skin = v[9];
      particle.positionAtLastBroadPhase = { x: v[10], y: v[11], z: v[12] };
    });
    Object.assign(this, { pairs: snapshot.pairs, candidateIds: snapshot.candidateIds, listVersion: snapshot.listVersion, collisionPairs: snapshot.collisionPairs, collisionIds: snapshot.collisionIds, newCollisionPairs: snapshot.newCollisionPairs, metrics: snapshot.metrics, events: snapshot.events, lastRebuild: snapshot.lastRebuild, previousCollisions: snapshot.previousCollisions, forceRebuild: snapshot.forceRebuild });
    this.triggerCounts.clear();
    snapshot.triggerCounts.forEach(([id, count]) => this.triggerCounts.set(id, count));
    this.config.K = snapshot.K;
    this.controller.K = snapshot.K;
    this.instrumentation.restoreHistory(snapshot.history);
    // 空間結構只在重建時建立；用「上次重建時的位置」重建一次，畫面上的格子、葉節點才會和那一幀一致
    if (this.metrics.step > 0) {
      const now = this.system.particles.map((particle) => particle.position);
      this.system.particles.forEach((particle) => { particle.position = particle.positionAtLastBroadPhase; });
      this.structure.build(this.system.particles);
      this.system.particles.forEach((particle, index) => { particle.position = now[index]; });
    }
    return true;
  }

  private saveSnapshot(): void {
    const particles = new Float64Array(this.system.particles.length * NUMBERS_PER_PARTICLE);
    this.system.particles.forEach(({ position: p, velocity: v, acceleration: a, skin, positionAtLastBroadPhase: s }, index) => {
      particles.set([p.x, p.y, p.z, v.x, v.y, v.z, a.x, a.y, a.z, skin, s.x, s.y, s.z], index * NUMBERS_PER_PARTICLE);
    });
    this.snapshots.push({ particles, pairs: this.pairs, candidateIds: this.candidateIds, listVersion: this.listVersion, collisionPairs: this.collisionPairs, collisionIds: this.collisionIds, newCollisionPairs: this.newCollisionPairs, metrics: this.metrics, events: this.events, triggerCounts: [...this.triggerCounts], lastRebuild: this.lastRebuild, previousCollisions: this.previousCollisions, forceRebuild: this.forceRebuild, K: this.controller.K, history: [...this.instrumentation.getHistory()] });
    if (this.snapshots.length > this.maxSnapshots) this.snapshots.shift();
  }

  // ===========================================================================
  // step()：跑「一步」模擬，對應論文 3.5 節與圖 2 的流程
  //   1. 判斷候選列表是否還有效（Verlet 判定）
  //   2. 無效 → 重建：建空間結構、更新 skin、拍快照、找候選配對（broad-phase）
  //      有效 → 跳過 broad-phase，沿用上次的候選配對
  //   3. narrow-phase：逐一確認候選配對是不是真的碰撞
  //   4. 碰撞回應（彈開）→ 更新速度與位置（積分）
  //   5. 記錄這一步的數據給畫面
  // ===========================================================================
  step(): StepMetrics {
    if (this.maxSnapshots > 0) this.saveSnapshot();
    const started = performance.now();
    const prev = this.metrics;
    const { bufferEnabled } = this.config;
    const particles = this.system.particles;
    // 1. 要不要重建？三種情況要重建：第一步（還沒有任何列表）、沒開 Verlet 緩衝（每步都重建）、
    //    或有粒子的累積位移超過自己的 skin。與 C++ Simulation::needsRebuild 相同：第 0 幀必定重建。
    //    另外，剛調整過 K 時也直接重建
    const checkStarted = performance.now();
    const needsRebuild = prev.step === 0 || !bufferEnabled || this.forceRebuild || !this.controller.isListValid(particles);
    const checkMs = performance.now() - checkStarted;
    this.forceRebuild = false;
    let broadPhaseMs = 0;
    let broadPhaseChecks = 0;
    if (needsRebuild) {
      // 2. broad-phase：重建空間結構與 skin，再找出候選配對，存在 pairs 裡給之後的步數沿用
      const broadPhaseStarted = performance.now();
      const event = this.controller.rebuild(this.structure, particles, prev.step + 1);
      this.pairs = this.structure.queryCandidatePairs(bufferEnabled);
      broadPhaseMs = performance.now() - broadPhaseStarted;
      // end broad-phase
      broadPhaseChecks = this.structure.getMetrics().distanceChecks;
      this.candidateIds = new Set(this.pairs.flat());
      this.listVersion += 1;
      // 只記錄 Verlet buffer 開啟、且確實有粒子超出自己 skin 而觸發的重建
      if (prev.step > 0 && bufferEnabled && event.displacement > event.skinAtTrigger) {
        this.triggerCounts.set(event.triggeredByParticleId, (this.triggerCounts.get(event.triggeredByParticleId) ?? 0) + 1);
        this.events = [event, ...this.events].slice(0, 12);
      }
    }

    // 3. narrow-phase：候選配對只是「可能」碰撞，這裡用真正的半徑和確認是否碰撞
    //    與 C++ narrow::colliding 相同：(r_a + r_b)² ≥ |pos_a − pos_b|²
    //    比較「平方」而不開根號，結果一樣但比較快
    const narrowPhaseStarted = performance.now();
    const collisionPairs = this.pairs.filter(([a, b]) => { const p = particles[a]; const q = particles[b]; const r = p.radius + q.radius; return r * r - ((p.position.x - q.position.x) ** 2 + (p.position.y - q.position.y) ** 2 + (p.position.z - q.position.z) ** 2) >= 0; });
    const narrowPhaseMs = performance.now() - narrowPhaseStarted;
    // end narrow-phase

    // 找出新碰撞：配對編號 = a × 粒子數 + b，和上一步的碰撞比對
    const n = particles.length;
    const current = new Set(collisionPairs.map(([a, b]) => a * n + b));
    this.newCollisionPairs = collisionPairs.filter(([a, b]) => !this.previousCollisions.has(a * n + b));
    this.previousCollisions = current;

    // 4. 與 C++ Simulation::step 相同：排序碰撞配對 → 碰撞回應 → 積分（含牆面反彈）
    //    排序是為了讓每次處理順序固定，結果才能和 C++ 版本一致
    collisionPairs.sort(([a1, b1], [a2, b2]) => a1 - a2 || b1 - b2);
    this.collisionPairs = collisionPairs;
    this.collisionIds = new Set(collisionPairs.flat());
    this.system.resolveCollisions(collisionPairs);
    this.system.step(DT);

    // 5. 記錄數據：沒重建的步數只做 narrow-phase，距離比對次數就是候選配對數
    const next: StepMetrics = {
      step: prev.step + 1, algorithm: this.config.algorithm, elapsedMs: performance.now() - started,
      checkMs, broadPhaseMs, narrowPhaseMs, detectionMs: checkMs + broadPhaseMs + narrowPhaseMs,
      broadPhaseChecks, distanceChecks: broadPhaseChecks + this.pairs.length, candidatePairs: this.pairs.length, collisions: collisionPairs.length,
      rebuilt: needsRebuild, rebuildCount: prev.rebuildCount + (needsRebuild ? 1 : 0), skippedSteps: prev.skippedSteps + (needsRebuild ? 0 : 1),
    };
    this.metrics = next;
    this.instrumentation.recordStep(next);
    if (needsRebuild && bufferEnabled && prev.step > 0) {
      this.lastRebuild = { step: next.step, rebuildCount: next.rebuildCount, triggerId: this.events[0]?.step === next.step ? this.events[0].triggeredByParticleId : null, detectionMs: next.detectionMs };
    }
    return next;
  }
}
