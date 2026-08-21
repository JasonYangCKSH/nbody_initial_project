import { add, scale, type ParticleData, type Vec3 } from './types';

export const PARTICLE_RADIUS = 0.075;

export class ParticleSystem {
  readonly particles: ParticleData[];
  constructor(count: number, readonly bounds: Vec3, seed = 17) {
    const random = () => { seed = (seed * 1664525 + 1013904223) >>> 0; return seed / 4294967296; };
    this.particles = Array.from({ length: count }, (_, id) => {
      const position = { x: random() * bounds.x, y: random() * bounds.y, z: random() * bounds.z };
      const velocity = { x: (random() - 0.5) * 1.5, y: (random() - 0.5) * 1.5, z: (random() - 0.5) * 1.5 };
      return { id, position, velocity, radius: PARTICLE_RADIUS, positionAtLastBroadPhase: { ...position }, skin: 0 };
    });
  }
  step(dt: number): void {
    this.particles.forEach((particle) => {
      particle.position = add(particle.position, scale(particle.velocity, dt));
      (['x', 'y', 'z'] as const).forEach((axis) => {
        if (particle.position[axis] < particle.radius) { particle.position[axis] = particle.radius; particle.velocity[axis] = Math.abs(particle.velocity[axis]); }
        if (particle.position[axis] > this.bounds[axis] - particle.radius) { particle.position[axis] = this.bounds[axis] - particle.radius; particle.velocity[axis] = -Math.abs(particle.velocity[axis]); }
      });
    });
  }
  resolveCollisions(pairs: [number, number][]): void {
    pairs.forEach(([i, j]) => {
      const a = this.particles[i];
      const b = this.particles[j];
      const dx = a.position.x - b.position.x;
      const dy = a.position.y - b.position.y;
      const dz = a.position.z - b.position.z;
      const dist = Math.hypot(dx, dy, dz) || 1e-6;
      const nx = dx / dist, ny = dy / dist, nz = dz / dist;
      const overlap = a.radius + b.radius - dist;
      if (overlap > 0) {
        const correction = overlap / 2;
        a.position.x += nx * correction; a.position.y += ny * correction; a.position.z += nz * correction;
        b.position.x -= nx * correction; b.position.y -= ny * correction; b.position.z -= nz * correction;
      }
      const relVelAlongNormal = (a.velocity.x - b.velocity.x) * nx + (a.velocity.y - b.velocity.y) * ny + (a.velocity.z - b.velocity.z) * nz;
      if (relVelAlongNormal > 0) return;
      a.velocity.x -= relVelAlongNormal * nx; a.velocity.y -= relVelAlongNormal * ny; a.velocity.z -= relVelAlongNormal * nz;
      b.velocity.x += relVelAlongNormal * nx; b.velocity.y += relVelAlongNormal * ny; b.velocity.z += relVelAlongNormal * nz;
    });
  }
}
