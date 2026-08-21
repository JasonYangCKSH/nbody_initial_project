import { add, scale, type ParticleData, type Vec3 } from './types';

export class ParticleSystem {
  readonly particles: ParticleData[];
  constructor(count: number, readonly bounds: Vec3, seed = 17) {
    const random = () => { seed = (seed * 1664525 + 1013904223) >>> 0; return seed / 4294967296; };
    this.particles = Array.from({ length: count }, (_, id) => {
      const position = { x: random() * bounds.x, y: random() * bounds.y, z: random() * bounds.z };
      const velocity = { x: (random() - 0.5) * 1.5, y: (random() - 0.5) * 1.5, z: (random() - 0.5) * 1.5 };
      return { id, position, velocity, radius: 0.075, positionAtLastBroadPhase: { ...position }, skin: 0 };
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
}
