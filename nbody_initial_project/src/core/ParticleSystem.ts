import { add, scale, type ParticleData, type Vec3 } from './types';
import { explosion, microScene, shellOf, spatialCluster } from './scenario';

// 每發煙火的顏色（避開碰撞的橘紅、觸發的紫色與選取的薄荷綠），快速火花用偏白的金色
export const SHELL_COLORS = ['#FF4D9D', '#FFD166', '#4CC9F0', '#FF8A3D', '#9BFF6A', '#7AE7FF', '#FF6B6B', '#F9F871'];
const SPARK_COLOR = '#FFF4D6';

export const PARTICLE_RADIUS = 0.075;
// 粒子雲每軸初速分量取自 [-speed, speed]
export const DEFAULT_SPEED = 0.75;
// 每軸加速度分量取自 [-acc, acc]，對應 C++ scenario::spatialCluster 的 acc 參數
export const DEFAULT_ACCELERATION = 0.25;
// 煙火爆炸場景中慢速粒子的基準速度
export const EXPLOSION_BURST_SPEED = 2.5;

// cloud：均勻／群聚粒子雲（clusterFactor 決定）；explosion：煙火爆炸（acc 當作向下的重力）；
// micro：小空間裡少量大球等速運動（劇本一，沒有加速度）
export type Scene = 'cloud' | 'explosion' | 'micro';

// radius：粒子半徑；speed：粒子雲是每軸初速範圍 ±speed，小空間是焦點球的速率
export interface ParticleOptions { radius?: number; speed?: number }

export class ParticleSystem {
  readonly particles: ParticleData[];
  // 每顆粒子的顯示顏色；只有煙火場景有，其他場景為 null（用預設顏色）
  readonly colors: string[] | null;
  // 焦點球：小空間場景的第 0、1 顆（安排好會正面相撞），畫面上加光圈；其他場景沒有
  readonly focusIds: number[];
  // clusters：粒子雲的群聚中心數，或煙火場景的煙火發數
  constructor(count: number, readonly bounds: Vec3, clusterFactor = 0, acc = DEFAULT_ACCELERATION, seed = 17, readonly scene: Scene = 'cloud', readonly clusters = scene === 'explosion' ? 3 : 1, { radius = PARTICLE_RADIUS, speed = DEFAULT_SPEED }: ParticleOptions = {}) {
    this.particles = scene === 'explosion'
      ? explosion({ count, bounds, radius, burstSpeed: EXPLOSION_BURST_SPEED, gravity: acc, shellCount: clusters, seed })
      : scene === 'micro'
        ? microScene({ count, bounds, radius, speed, seed })
        : spatialCluster({
          count, bounds, radius, speed, acc, clusterFactor,
          hotspotSpread: 0.03 * bounds.x, hotspotCount: clusters, seed,
        });
    // 依初始速度判斷是否為快速火花（慢速粒子最多 1 倍、火花至少 2 倍 burstSpeed）
    this.colors = scene === 'explosion'
      ? this.particles.map(({ id, velocity: v }) => (Math.hypot(v.x, v.y, v.z) > EXPLOSION_BURST_SPEED * 1.5 ? SPARK_COLOR : SHELL_COLORS[shellOf(id, clusters) % SHELL_COLORS.length]))
      : null;
    this.focusIds = scene === 'micro' ? [0, 1].filter((id) => id < count) : [];
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
