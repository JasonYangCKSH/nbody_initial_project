import { add, scale, type ParticleData, type Vec3 } from './types';
import { spatialCluster } from './scenario';

export const PARTICLE_RADIUS = 0.075;
// 每軸加速度分量取自 [-acc, acc]，對應 C++ scenario::spatialCluster 的 acc 參數
export const DEFAULT_ACCELERATION = 0.25;

export class ParticleSystem {
  readonly particles: ParticleData[];
  constructor(count: number, readonly bounds: Vec3, clusterFactor = 0, acc = DEFAULT_ACCELERATION, seed = 17) {
    this.particles = spatialCluster({
      count, bounds, radius: PARTICLE_RADIUS, speed: 0.75, acc, clusterFactor,
      hotspotSpread: 0.03 * bounds.x, hotspotCount: 1, seed,
    });
  }
  // 與 C++ Simulation::integrate 相同：先更新速度、再更新位置，最後做牆面反彈
  step(dt: number): void {
    this.particles.forEach((particle) => {
      particle.velocity = add(particle.velocity, scale(particle.acceleration, dt));
      particle.position = add(particle.position, scale(particle.velocity, dt));
    });
    this.reflectOffWalls();
  }
  // 移植自 C++ response::reflectOffWalls。C++ 的世界範圍是 [-worldSize/2, worldSize/2]，這裡是 [0, bounds]
  reflectOffWalls(): void {
    this.particles.forEach((particle) => {
      (['x', 'y', 'z'] as const).forEach((axis) => {
        const lo = particle.radius;
        const hi = this.bounds[axis] - particle.radius;
        if (particle.position[axis] < lo) {
          particle.position[axis] = lo;
          particle.velocity[axis] = -particle.velocity[axis];
        } else if (particle.position[axis] > hi) {
          particle.position[axis] = hi;
          particle.velocity[axis] = -particle.velocity[axis];
        }
      });
    });
  }
  // 移植自 C++ response::resolveCollisions：一般質量彈性碰撞衝量，接近中的配對才施加衝量並依質量反比修正重疊
  resolveCollisions(pairs: [number, number][], restitution = 1): void {
    pairs.forEach(([i, j]) => {
      const a = this.particles[i];
      const b = this.particles[j];
      const dx = b.position.x - a.position.x;
      const dy = b.position.y - a.position.y;
      const dz = b.position.z - a.position.z;
      const dist = Math.hypot(dx, dy, dz);
      if (dist < 1e-6) return; // 位置重合，法線無意義，避免除以零
      const nx = dx / dist, ny = dy / dist, nz = dz / dist;

      const velAlongNormal = (a.velocity.x - b.velocity.x) * nx + (a.velocity.y - b.velocity.y) * ny + (a.velocity.z - b.velocity.z) * nz;
      if (velAlongNormal <= 0) return; // 已經在分離或平行，不需要施加衝量

      const invMassA = 1 / a.mass;
      const invMassB = 1 / b.mass;
      const impulseMag = -(1 + restitution) * velAlongNormal / (invMassA + invMassB);
      a.velocity.x += impulseMag * nx * invMassA; a.velocity.y += impulseMag * ny * invMassA; a.velocity.z += impulseMag * nz * invMassA;
      b.velocity.x -= impulseMag * nx * invMassB; b.velocity.y -= impulseMag * ny * invMassB; b.velocity.z -= impulseMag * nz * invMassB;

      const overlap = a.radius + b.radius - dist;
      if (overlap > 0) {
        const correction = overlap / (invMassA + invMassB);
        a.position.x -= nx * correction * invMassA; a.position.y -= ny * correction * invMassA; a.position.z -= nz * correction * invMassA;
        b.position.x += nx * correction * invMassB; b.position.y += ny * correction * invMassB; b.position.z += nz * correction * invMassB;
      }
    });
  }
}
