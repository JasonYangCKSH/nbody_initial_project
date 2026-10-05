import { distance, type DebugGeometry, type ParticleData, type SpatialStructure } from '../types';

type Vec3 = { x: number; y: number; z: number };

type Node = {
  center: Vec3;
  halfExtent: number;
  indices: number[];
  children: Array<Node | null>;
};

const childIndex = (center: Vec3, pos: Vec3): number => {
  let idx = 0;
  if (pos.x >= center.x) idx |= 1;
  if (pos.y >= center.y) idx |= 2;
  if (pos.z >= center.z) idx |= 4;
  return idx;
};

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

  constructor(
    private readonly bounds: Vec3,
    private readonly maxDepth = 6,
    private readonly leafCapacity = 8,
  ) {}

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

  getDebugGeometry(): DebugGeometry { return { kind: 'grid', cellSize: 0, bounds: this.bounds }; }

  getMetrics() { return { distanceChecks: this.distanceChecks, candidatePairs: this.candidatePairs }; }

  // Must be called after build(): each particle's cap comes from the half-extent of the leaf it landed in.
  getSkinCaps(particles: ParticleData[]): number[] {
    const caps = new Array(particles.length).fill(0);
    if (!this.root) return caps;
    for (const leaf of this.collectLeaves(this.root)) {
      for (const index of leaf.indices) caps[index] = Math.max(leaf.halfExtent - particles[index].radius, 0);
    }
    return caps;
  }

  private insert(node: Node, index: number, depth: number): void {
    if (node.children.every((child) => child === null)) {
      node.indices.push(index);
      if (node.indices.length > this.leafCapacity && depth < this.maxDepth) {
        this.split(node, depth);
      }
      return;
    }

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

    this.insert(next, index, depth + 1);
  }

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

  private boxesOverlap(a: Node, b: Node, margin: number): boolean {
    const dx = Math.abs(a.center.x - b.center.x);
    const dy = Math.abs(a.center.y - b.center.y);
    const dz = Math.abs(a.center.z - b.center.z);
    const reach = a.halfExtent + b.halfExtent + margin;
    return dx <= reach && dy <= reach && dz <= reach;
  }

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
