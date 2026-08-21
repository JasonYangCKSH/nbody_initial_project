import { distance, type DebugGeometry, type ParticleData, type SpatialStructure } from '../types';

type CellCoord = { x: number; y: number; z: number };
type Cell = { coord: CellCoord; indices: number[] };

// The 13 "forward" neighbour offsets plus the cell itself visit every
// unordered pair of cells exactly once (half-shell stencil), so candidate
// pairs never need to be de-duplicated. Mirrors broad::LinkedCell in
// collision/include/broad_phase.h.
const FORWARD_OFFSETS: CellCoord[] = [
  { x: 1, y: 0, z: 0 }, { x: 1, y: 1, z: 0 }, { x: 0, y: 1, z: 0 }, { x: -1, y: 1, z: 0 },
  { x: 1, y: 0, z: -1 }, { x: 1, y: 1, z: -1 }, { x: 0, y: 1, z: -1 }, { x: -1, y: 1, z: -1 },
  { x: 1, y: 0, z: 1 }, { x: 1, y: 1, z: 1 }, { x: 0, y: 1, z: 1 }, { x: -1, y: 1, z: 1 },
  { x: 0, y: 0, z: 1 },
];

export class UniformGridStructure implements SpatialStructure {
  private particles: ParticleData[] = [];
  private cells = new Map<string, Cell>();
  private distanceChecks = 0;
  private candidatePairs = 0;

  constructor(private readonly bounds: { x: number; y: number; z: number }, private readonly cellSize: number) {}

  build(particles: ParticleData[]): void {
    this.particles = particles;
    this.cells = new Map();
    particles.forEach((particle, index) => {
      const coord = this.cellOf(particle.position);
      const key = this.cellKey(coord);
      const cell = this.cells.get(key);
      if (cell) cell.indices.push(index); else this.cells.set(key, { coord, indices: [index] });
    });
    this.distanceChecks = 0;
    this.candidatePairs = 0;
  }

  queryCandidatePairs(withSkin = false): [number, number][] {
    const pairs: [number, number][] = [];
    this.distanceChecks = 0;
    for (const { coord, indices } of this.cells.values()) {
      for (let a = 0; a < indices.length; a += 1) {
        for (let b = a + 1; b < indices.length; b += 1) this.tryAdd(indices[a], indices[b], withSkin, pairs);
      }

      for (const offset of FORWARD_OFFSETS) {
        const neighbor = this.cells.get(this.cellKey({ x: coord.x + offset.x, y: coord.y + offset.y, z: coord.z + offset.z }));
        if (!neighbor) continue;
        for (const first of indices) for (const second of neighbor.indices) this.tryAdd(first, second, withSkin, pairs);
      }
    }
    this.candidatePairs = pairs.length;
    return pairs;
  }

  getDebugGeometry(): DebugGeometry { return { kind: 'grid', cellSize: this.cellSize, bounds: this.bounds }; }
  getMetrics() { return { distanceChecks: this.distanceChecks, candidatePairs: this.candidatePairs }; }

  private tryAdd(first: number, second: number, withSkin: boolean, pairs: [number, number][]) {
    const a = this.particles[first];
    const b = this.particles[second];
    this.distanceChecks += 1;
    const threshold = a.radius + b.radius + (withSkin ? a.skin + b.skin : 0);
    if (distance(a.position, b.position) <= threshold) pairs.push(first < second ? [first, second] : [second, first]);
  }

  private cellOf(position: { x: number; y: number; z: number }): CellCoord {
    return { x: Math.floor(position.x / this.cellSize), y: Math.floor(position.y / this.cellSize), z: Math.floor(position.z / this.cellSize) };
  }

  private cellKey(coord: CellCoord) { return `${coord.x}:${coord.y}:${coord.z}`; }
}
