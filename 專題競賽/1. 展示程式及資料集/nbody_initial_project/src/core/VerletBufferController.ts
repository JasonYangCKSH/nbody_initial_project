// =============================================================================
// Verlet 緩衝（Verlet List）控制器：決定「這一步要不要重建候選列表」
// -----------------------------------------------------------------------------
// 問題：每一步都重新建空間結構、重新找候選配對（broad-phase）很花時間。
// 想法（時間連貫性）：粒子每一步只移動一點點，上一步找到的候選配對多半還能用。
//   1. 重建時，每顆粒子多算一圈「緩衝距離」skin，找出「接下來幾步內可能碰撞」的配對
//   2. 之後每一步只檢查：有沒有任何粒子移動超過自己的 skin？
//      - 沒有 → 沿用舊的候選列表，跳過 broad-phase（畫面上的「跳過比例」）
//      - 有   → 舊列表可能漏抓，必須重建
// 本檔對應論文 3.3 節（動態 skin、位移失效判定與快照機制）。
// =============================================================================
import { distance, type ParticleData, type RebuildEvent, type SpatialStructure } from './types';

export class VerletBufferController {
  public RC: number;
  // K：預測幾步之後的位移（論文的「跨步係數」）。K 越大 skin 越大，越少重建，但候選配對越多
  public K: number;
  private step = 0;
  private lastTrigger = 0;

  constructor(RC: number, K: number, private readonly dt: number) { this.RC = RC; this.K = K; }

  // 論文式(1)：skin = K·|v|·Δt + ½·|a|·(K·Δt)²
  // 就是高中物理的位移公式 s = v·t + ½·a·t²，把 t 換成「K 步的時間」K·Δt，
  // 預測這顆粒子在接下來 K 步內最多會移動多遠。與 C++ verlet::updateLocalSkin 一致。
  computeSkin(particle: ParticleData): number {
    const v = Math.hypot(particle.velocity.x, particle.velocity.y, particle.velocity.z);         // 速度大小 |v|
    const a = Math.hypot(particle.acceleration.x, particle.acceleration.y, particle.acceleration.z); // 加速度大小 |a|
    const horizon = this.K * this.dt;   // 預測的時間長度 t = K·Δt
    return v * horizon + 0.5 * a * horizon * horizon;
  }

  // 幫每顆粒子算新的 skin；如果空間結構有上限（論文式(3)/(4)），skin 不能超過上限
  updateSkins(particles: ParticleData[], caps?: number[]): void {
    particles.forEach((particle, index) => {
      particle.skin = this.computeSkin(particle);
      if (caps) particle.skin = Math.min(particle.skin, caps[index]);
    });
  }

  // 位移失效判定（論文式(2)）：每顆粒子的累積位移 Δx = |現在位置 − 快照位置|。
  // 「全部粒子」都滿足 Δx ≤ skin 才回傳 true（舊列表仍有效）；只要有一顆超過就是 false。
  isListValid(particles: ParticleData[]): boolean { return particles.every((particle) => distance(particle.position, particle.positionAtLastBroadPhase) <= particle.skin); }

  // 重建：順序很重要，對應論文 3.5 節與圖 2 的「重建路徑」
  rebuild(structure: SpatialStructure, particles: ParticleData[], step: number): RebuildEvent {
    this.step = step;
    // (0) 先記錄是誰觸發了重建（只為了顯示在事件記錄，不影響計算）：
    //     找出「超出自己 skin 最多」的粒子，也就是 Δx − skin 最大的那顆。
    //     必須在更新 skin 之前做，因為要用的是「舊的」skin。
    //     第一次建立時還沒有舊 skin，結果無意義但不影響。
    const trigger = particles.reduce((best, particle) => {
      const displacement = distance(particle.position, particle.positionAtLastBroadPhase);
      const excess = displacement - particle.skin;
      return excess > best.excess ? { particle, displacement, skin: particle.skin, excess } : best;
    }, { particle: particles[0], displacement: 0, skin: particles[0]?.skin ?? 0, excess: -Infinity });
    // (1) 重建空間結構（分格子或建八元樹）
    structure.build(particles);
    // (2) 依速度、加速度算新的 skin，並套用空間結構給的上限（例如八元樹的葉節點半邊長 − 半徑）
    this.updateSkins(particles, structure.getSkinCaps?.(particles));
    // (3) 拍快照：記下現在的位置，之後的累積位移從這裡開始算。{ ...x } 是複製一份，避免之後跟著被改
    particles.forEach((particle) => { particle.positionAtLastBroadPhase = { ...particle.position }; });
    // 注意：「找候選配對」(queryCandidatePairs) 是在 App.tsx 的 step() 裡，緊接在 rebuild 之後呼叫
    this.lastTrigger = trigger.particle?.id ?? 0;
    return { step, triggeredByParticleId: this.lastTrigger, displacement: trigger.displacement, skinAtTrigger: trigger.skin };
  }
}
