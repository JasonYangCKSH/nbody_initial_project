// =============================================================================
// 均勻網格（Uniform Grid）：把空間切成一樣大的小方格
// -----------------------------------------------------------------------------
// 想法：距離很遠的粒子不可能碰撞，所以只需要比「同一格」和「相鄰格」的粒子。
// 1. build：算出每顆粒子在哪一格，把粒子編號放進那一格的清單
// 2. query：每一格只和自己、以及旁邊的格子比
// 只要格子邊長 ≥ 兩顆粒子能碰到的最遠距離，就保證不會漏抓。
// 平均情況下接近 O(n)，比暴力法快非常多。
// =============================================================================
import { distance, type DebugGeometry, type ParticleData, type SpatialStructure } from '../types';

type CellCoord = { x: number; y: number; z: number };     // 格子的整數座標，例如 (3, 0, -2)
type Cell = { coord: CellCoord; indices: number[] };       // 一格：座標 + 格子裡有哪些粒子（存編號）

// 3D 中一格周圍有 26 個鄰居。如果每格都和 26 個鄰居比，A 比 B、B 又比 A，會重複。
// 這裡只取「前方」的 13 個鄰居（加上自己這格），剛好讓每一對格子只被比一次，
// 所以不用再去除重複的配對。對應 C++ collision/include/broad_phase.h 的 broad::LinkedCell。
const FORWARD_OFFSETS: CellCoord[] = [
  { x: 1, y: 0, z: 0 }, { x: 1, y: 1, z: 0 }, { x: 0, y: 1, z: 0 }, { x: -1, y: 1, z: 0 },
  { x: 1, y: 0, z: -1 }, { x: 1, y: 1, z: -1 }, { x: 0, y: 1, z: -1 }, { x: -1, y: 1, z: -1 },
  { x: 1, y: 0, z: 1 }, { x: 1, y: 1, z: 1 }, { x: 0, y: 1, z: 1 }, { x: -1, y: 1, z: 1 },
  { x: 0, y: 0, z: 1 },
];

export class UniformGridStructure implements SpatialStructure {
  private particles: ParticleData[] = [];
  // 用 Map（雜湊表）存格子：key 是 "x:y:z" 字串，只存「有粒子的格子」，空格子不佔記憶體
  private cells = new Map<string, Cell>();
  private distanceChecks = 0;
  private candidatePairs = 0;

  // bounds：模擬空間大小；cellSize：格子邊長
  constructor(private readonly bounds: { x: number; y: number; z: number }, private readonly cellSize: number) {}

  // 把每顆粒子放進它所在的格子
  build(particles: ParticleData[]): void {
    this.particles = particles;
    this.cells = new Map();
    particles.forEach((particle, index) => {
      const coord = this.cellOf(particle.position);   // 這顆粒子在哪一格
      const key = this.cellKey(coord);
      const cell = this.cells.get(key);
      // 格子已存在就加進清單；不存在就新建一格
      if (cell) cell.indices.push(index); else this.cells.set(key, { coord, indices: [index] });
    });
    this.distanceChecks = 0;
    this.candidatePairs = 0;
  }

  queryCandidatePairs(withSkin = false): [number, number][] {
    const pairs: [number, number][] = [];
    this.distanceChecks = 0;
    for (const { coord, indices } of this.cells.values()) {
      // (1) 同一格內的粒子兩兩比較（和暴力法一樣，只是範圍縮小到一格）
      for (let a = 0; a < indices.length; a += 1) {
        for (let b = a + 1; b < indices.length; b += 1) this.tryAdd(indices[a], indices[b], withSkin, pairs);
      }

      // (2) 和 13 個「前方」鄰居格比較
      for (const offset of FORWARD_OFFSETS) {
        const neighbor = this.cells.get(this.cellKey({ x: coord.x + offset.x, y: coord.y + offset.y, z: coord.z + offset.z }));
        if (!neighbor) continue;   // 鄰居格沒有粒子，跳過
        for (const first of indices) for (const second of neighbor.indices) this.tryAdd(first, second, withSkin, pairs);
      }
    }
    this.candidatePairs = pairs.length;
    return pairs;
  }

  getDebugGeometry(): DebugGeometry { return { kind: 'grid', cellSize: this.cellSize, bounds: this.bounds }; }
  getMetrics() { return { distanceChecks: this.distanceChecks, candidatePairs: this.candidatePairs }; }
  // skin 上限（論文式(3)）：skin_max = max(0, h/2 − r)。
  // 因為只搜尋相鄰格，粒子若移動超過半格，可能跑到搜尋範圍外而漏抓，所以 skin 不能太大。
  getSkinCaps(particles: ParticleData[]): number[] { return particles.map((particle) => Math.max(this.cellSize / 2 - particle.radius, 0)); }

  // 比一對粒子的距離，夠近就加入候選名單（判斷方式和暴力法相同）
  private tryAdd(first: number, second: number, withSkin: boolean, pairs: [number, number][]) {
    const a = this.particles[first];
    const b = this.particles[second];
    this.distanceChecks += 1;
    const threshold = a.radius + b.radius + (withSkin ? a.skin + b.skin : 0);
    // 一律存成「小編號在前」，才能和暴力法的結果直接比對
    if (distance(a.position, b.position) <= threshold) pairs.push(first < second ? [first, second] : [second, first]);
  }

  // 座標 ÷ 格子邊長，再無條件捨去，就是格子編號。例如 cellSize = 0.3，x = 1.0 → 第 3 格
  private cellOf(position: { x: number; y: number; z: number }): CellCoord {
    return { x: Math.floor(position.x / this.cellSize), y: Math.floor(position.y / this.cellSize), z: Math.floor(position.z / this.cellSize) };
  }

  // 把格子座標變成字串當作 Map 的 key，例如 (3, 0, -2) → "3:0:-2"
  private cellKey(coord: CellCoord) { return `${coord.x}:${coord.y}:${coord.z}`; }
}
