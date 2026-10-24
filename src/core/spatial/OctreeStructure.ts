// =============================================================================
// 八元樹（Octree）：會依粒子密度自動調整格子大小的空間結構
// -----------------------------------------------------------------------------
// 均勻網格的問題：格子大小固定。粒子擠在一起的地方，一格裡塞了上百顆，又變回暴力法。
// 八元樹的做法：
//   - 從一個包住整個空間的大方塊（根節點，第 0 層）開始
//   - 某個方塊裡的粒子超過容量 leafCapacity，就把它切成 8 個一半大小的小方塊
//     （3D 每個軸各切一刀：2 × 2 × 2 = 8，所以叫「八」元樹）
//   - 小方塊還是太擠就繼續切，直到粒子夠少，或到達最大深度 maxDepth
// 結果：粒子密集處切得很細（深層、小方塊），稀疏處保持大方塊（淺層）。
// 最後沒有再切的方塊叫「葉節點」，粒子都放在葉節點裡。
// =============================================================================
import { distance, type DebugGeometry, type ParticleData, type SpatialStructure } from '../types';

type Vec3 = { x: number; y: number; z: number };

// 樹上的一個節點（一個方塊）
type Node = {
  center: Vec3;                 // 方塊中心
  halfExtent: number;           // 半邊長（中心到邊的距離）；每往下一層減半
  indices: number[];            // 方塊裡的粒子編號（只有葉節點會有）
  children: Array<Node | null>; // 8 個子方塊；全部是 null 代表這是葉節點
};

// 判斷一個位置落在 8 個子方塊中的哪一個（0～7）。
// 用 3 個位元表示：x 在中心右邊 → 第 1 位元為 1，y 在上面 → 第 2 位元，z 在前面 → 第 3 位元。
// 例如 x 右、y 下、z 前 → 1 + 0 + 4 = 5。「|=」是位元 OR，把那一位設成 1。
const childIndex = (center: Vec3, pos: Vec3): number => {
  let idx = 0;
  if (pos.x >= center.x) idx |= 1;
  if (pos.y >= center.y) idx |= 2;
  if (pos.z >= center.z) idx |= 4;
  return idx;
};

// 算第 childIdx 個子方塊的中心：從父方塊中心往各軸正或負方向移動「四分之一邊長」（= 父半邊長的一半）
const childCenter = (parentCenter: Vec3, parentHalfExtent: number, childIdx: number): Vec3 => {
  const quarter = parentHalfExtent * 0.5;
  return {
    x: parentCenter.x + ((childIdx & 1) ? quarter : -quarter),
    y: parentCenter.y + ((childIdx & 2) ? quarter : -quarter),
    z: parentCenter.z + ((childIdx & 4) ? quarter : -quarter),
  };
};

export class OctreeStructure implements SpatialStructure {
  private particles: ParticleData[] = [];
  private root: Node | null = null;
  private distanceChecks = 0;
  private candidatePairs = 0;

  // maxDepth：最多切幾層；leafCapacity：一個葉節點最多放幾顆粒子，超過就切開
  constructor(
    private readonly bounds: Vec3,
    private readonly maxDepth = 6,
    private readonly leafCapacity = 8,
  ) {}

  // 建樹：先做一個包住整個模擬空間的根方塊，再把粒子一顆一顆插進去
  build(particles: ParticleData[]): void {
    this.particles = particles;
    const extent = Math.max(this.bounds.x, this.bounds.y, this.bounds.z);
    this.root = {
      center: { x: this.bounds.x / 2, y: this.bounds.y / 2, z: this.bounds.z / 2 },
      halfExtent: extent / 2,
      indices: [],
      children: Array.from({ length: 8 }, () => null),
    };

    particles.forEach((_, index) => this.insert(this.root!, index, 0));
    this.distanceChecks = 0;
    this.candidatePairs = 0;
  }

  queryCandidatePairs(withSkin = false): [number, number][] {
    const pairs: [number, number][] = [];
    this.distanceChecks = 0;
    if (!this.root) return pairs;

    // 找候選配對的方式：
    //   (1) 同一個葉節點內的粒子兩兩比較
    //   (2) 兩個葉節點「靠得夠近」（方塊外擴 margin 後有重疊），就把兩邊的粒子互相比較
    // margin = 2 × 粒子最遠能碰到的距離（半徑 + skin），保證相鄰方塊裡可能碰撞的粒子不會漏掉
    const leaves = this.collectLeaves(this.root);
    const maxReach = this.particles.reduce((max, particle) => {
      const skin = withSkin ? particle.skin : 0;
      return Math.max(max, particle.radius + skin);
    }, 0);
    const margin = 2 * maxReach;

    for (let i = 0; i < leaves.length; i += 1) {
      const leafA = leaves[i];
      for (let a = 0; a < leafA.indices.length; a += 1) {
        for (let b = a + 1; b < leafA.indices.length; b += 1) {
          this.tryAdd(leafA.indices[a], leafA.indices[b], withSkin, pairs);
        }
      }

      for (let j = i + 1; j < leaves.length; j += 1) {
        const leafB = leaves[j];
        if (!this.boxesOverlap(leafA, leafB, margin)) continue;
        for (const a of leafA.indices) {
          for (const b of leafB.indices) {
            this.tryAdd(a, b, withSkin, pairs);
          }
        }
      }
    }

    this.candidatePairs = pairs.length;
    return pairs;
  }

  getDebugGeometry(): DebugGeometry {
    const root = this.root;
    const boxes = root ? this.collectLeaves(root).map((leaf) => ({ center: leaf.center, halfExtent: leaf.halfExtent, depth: this.depthOf(root, leaf) })) : [];
    const splits: { center: Vec3; halfExtent: number; depth: number }[] = [];
    if (root) this.collectSplits(root, 0, splits);
    return { kind: 'octree', boxes, splits };
  }

  getMetrics() { return { distanceChecks: this.distanceChecks, candidatePairs: this.candidatePairs }; }

  // 粒子在最近一次 build() 時所在的葉節點；depth 由半邊長推得，根節點為第 0 層，count 為葉節點內的粒子數
  getLeafOf(index: number): { center: Vec3; halfExtent: number; depth: number; count: number } | null {
    if (!this.root) return null;
    const leaf = this.collectLeaves(this.root).find((node) => node.indices.includes(index));
    if (!leaf) return null;
    return { center: leaf.center, halfExtent: leaf.halfExtent, depth: this.depthOf(this.root, leaf), count: leaf.indices.length };
  }

  // 葉節點容量規則：粒子數超過 leafCapacity 就切成 8 格；但已到 maxDepth 的葉節點不再切，因此不受容量限制
  getLimits(): { leafCapacity: number; maxDepth: number } { return { leafCapacity: this.leafCapacity, maxDepth: this.maxDepth }; }

  // 每顆粒子在最近一次 build() 時所在葉節點的層級（索引對應 particles）；尚未建樹時回傳空陣列
  getLeafDepths(): number[] {
    const root = this.root;
    if (!root) return [];
    const depths: number[] = new Array(this.particles.length).fill(-1);
    for (const leaf of this.collectLeaves(root)) {
      const depth = this.depthOf(root, leaf);
      for (const index of leaf.indices) depths[index] = depth;
    }
    return depths;
  }

  // 每往下一層半邊長減半，所以層級 = log2(根半邊長 / 葉半邊長)
  private depthOf(root: Node, leaf: Node): number { return Math.round(Math.log2(root.halfExtent / leaf.halfExtent)); }

  // skin 上限（論文式(4)）：skin_max = max(0, l − r)，l 是粒子所在葉節點的半邊長。
  // 密集處葉節點小 → skin 上限小；稀疏處葉節點大 → 上限也放寬。必須在 build() 之後呼叫。
  // 注意：葉節點的半邊長若小於粒子半徑，上限就是 0，這顆粒子只要一動就會觸發重建。
  getSkinCaps(particles: ParticleData[]): number[] {
    const caps = new Array(particles.length).fill(0);
    if (!this.root) return caps;
    for (const leaf of this.collectLeaves(this.root)) {
      for (const index of leaf.indices) caps[index] = Math.max(leaf.halfExtent - particles[index].radius, 0);
    }
    return caps;
  }

  // 把第 index 顆粒子插進 node 這個方塊（遞迴：函式會呼叫自己往下一層找）
  private insert(node: Node, index: number, depth: number): void {
    // 情況 A：node 是葉節點 → 直接放進來；放完如果超過容量、又還沒到最大深度，就切開
    if (node.children.every((child) => child === null)) {
      node.indices.push(index);
      if (node.indices.length > this.leafCapacity && depth < this.maxDepth) {
        this.split(node, depth);
      }
      return;
    }

    // 情況 B：node 已經被切開過 → 找出粒子屬於哪個子方塊，交給那個子方塊處理
    const child = childIndex(node.center, this.particles[index].position);
    const next = node.children[child];
    if (!next) {
      const childNode: Node = {
        center: childCenter(node.center, node.halfExtent, child),
        halfExtent: node.halfExtent * 0.5,
        indices: [],
        children: Array.from({ length: 8 }, () => null),
      };
      node.children[child] = childNode;
      childNode.indices.push(index);
      return;
    }

    this.insert(next, index, depth + 1); // 往下一層繼續插
  }

  // 把一個太擠的葉節點切成 8 個子方塊，再把原本的粒子重新插進去（它們會各自落到某個子方塊）
  private split(node: Node, depth: number): void {
    const existing = [...node.indices];
    node.indices = [];
    node.children = Array.from({ length: 8 }, () => null);

    for (let childIndexValue = 0; childIndexValue < 8; childIndexValue += 1) {
      const childNode: Node = {
        center: childCenter(node.center, node.halfExtent, childIndexValue),
        halfExtent: node.halfExtent * 0.5,
        indices: [],
        children: Array.from({ length: 8 }, () => null),
      };
      node.children[childIndexValue] = childNode;
    }

    for (const idx of existing) {
      this.insert(node, idx, depth + 1);
    }
  }

  // 走訪整棵樹，收集所有「有粒子的葉節點」
  private collectLeaves(node: Node): Node[] {
    if (node.children.every((child) => child === null)) {
      return node.indices.length > 0 ? [node] : [];
    }

    const leaves: Node[] = [];
    for (const child of node.children) {
      if (child) leaves.push(...this.collectLeaves(child));
    }
    return leaves;
  }

  private collectSplits(node: Node, depth: number, out: { center: Vec3; halfExtent: number; depth: number }[]): void {
    if (node.children.every((child) => child === null)) return;
    out.push({ center: node.center, halfExtent: node.halfExtent, depth });
    for (const child of node.children) {
      if (child) this.collectSplits(child, depth + 1, out);
    }
  }

  // 兩個方塊各自往外擴 margin 之後，在 x、y、z 三個軸上是否都有重疊（AABB 重疊判斷）
  private boxesOverlap(a: Node, b: Node, margin: number): boolean {
    const dx = Math.abs(a.center.x - b.center.x);
    const dy = Math.abs(a.center.y - b.center.y);
    const dz = Math.abs(a.center.z - b.center.z);
    const reach = a.halfExtent + b.halfExtent + margin;
    return dx <= reach && dy <= reach && dz <= reach;
  }

  // 比一對粒子的距離，夠近就加入候選名單（判斷方式和暴力法相同）
  private tryAdd(first: number, second: number, withSkin: boolean, pairs: [number, number][]): void {
    if (first === second) return;
    const a = this.particles[first];
    const b = this.particles[second];
    this.distanceChecks += 1;
    const threshold = a.radius + b.radius + (withSkin ? a.skin + b.skin : 0);
    if (distance(a.position, b.position) <= threshold) {
      pairs.push(first < second ? [first, second] : [second, first]);
    }
  }
}
