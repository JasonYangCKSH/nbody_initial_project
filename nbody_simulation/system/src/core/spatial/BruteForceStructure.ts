import { distance, type DebugGeometry, type ParticleData, type SpatialStructure } from '../types';

export class BruteForceStructure implements SpatialStructure {
  private particles: ParticleData[] = [];
  private distanceChecks = 0;
  private candidatePairs = 0;

  build(particles: ParticleData[]): void {
    this.particles = particles;
    this.distanceChecks = 0;
    this.candidatePairs = 0;
  }

  queryCandidatePairs(withSkin = false): [number, number][] {
    const pairs: [number, number][] = [];
    this.distanceChecks = 0;
    for (let first = 0; first < this.particles.length; first += 1) {
      for (let second = first + 1; second < this.particles.length; second += 1) {
        const a = this.particles[first];
        const b = this.particles[second];
        this.distanceChecks += 1;
        const threshold = a.radius + b.radius + (withSkin ? a.skin + b.skin : 0);
        if (distance(a.position, b.position) <= threshold) pairs.push([first, second]);
      }
    }
    this.candidatePairs = pairs.length;
    return pairs;
  }

  getDebugGeometry(): DebugGeometry { return { kind: 'grid', cellSize: 0, bounds: { x: 0, y: 0, z: 0 } }; }
  getMetrics() { return { distanceChecks: this.distanceChecks, candidatePairs: this.candidatePairs }; }
}
