// =============================================================================
// 暴力法（Brute Force）：最直接、最慢、但一定正確的做法
// -----------------------------------------------------------------------------
// 把每一顆粒子和其他「每一顆」粒子都比一次距離。
// n 顆粒子要比 n×(n−1)/2 次，例如 14,100 顆 → 約 9,940 萬次，這就是 O(n²)。
// 它的用途是當「標準答案」：其他方法找到的配對必須和它一模一樣（見 core.test.ts）。
// =============================================================================
import { distance, type DebugGeometry, type ParticleData, type SpatialStructure } from '../types';

export class BruteForceStructure implements SpatialStructure {
  private particles: ParticleData[] = [];
  private distanceChecks = 0;   // 統計：做了幾次距離計算
  private candidatePairs = 0;   // 統計：找到幾組候選配對

  // 暴力法不需要任何資料結構，記住粒子就好
  build(particles: ParticleData[]): void {
    this.particles = particles;
    this.distanceChecks = 0;
    this.candidatePairs = 0;
  }

  queryCandidatePairs(withSkin = false): [number, number][] {
    const pairs: [number, number][] = [];
    this.distanceChecks = 0;
    // 雙層迴圈：second 從 first + 1 開始，所以 (0,1) 比過就不會再比 (1,0)，也不會自己比自己
    for (let first = 0; first < this.particles.length; first += 1) {
      for (let second = first + 1; second < this.particles.length; second += 1) {
        const a = this.particles[first];
        const b = this.particles[second];
        this.distanceChecks += 1;
        // 判斷門檻：兩個半徑相加；有 skin 時再加上兩顆的 skin，
        // 代表「就算兩顆都往對方移動 skin 這麼遠，也可能撞到」→ 先放進候選名單
        const threshold = a.radius + b.radius + (withSkin ? a.skin + b.skin : 0);
        if (distance(a.position, b.position) <= threshold) pairs.push([first, second]);
      }
    }
    this.candidatePairs = pairs.length;
    return pairs;
  }

  getDebugGeometry(): DebugGeometry { return { kind: 'grid', cellSize: 0, bounds: { x: 0, y: 0, z: 0 } }; }
  getMetrics() { return { distanceChecks: this.distanceChecks, candidatePairs: this.candidatePairs }; }
  // 沒有 getSkinCaps：暴力法每對都比，不管粒子跑多遠都不會漏抓，所以 skin 不需要上限
}
