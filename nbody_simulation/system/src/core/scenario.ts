import type { ParticleData, Vec3 } from './types';

export interface SpatialClusterOptions {
  count: number;
  bounds: Vec3;
  radius: number;
  speed: number;
  acc: number;
  // 0 = 全部均勻分布（等同 uniformCloud），1 = 全部落在 hotspot 附近
  clusterFactor: number;
  hotspotSpread: number;
  hotspotCount: number;
  fastRatio?: number;
  fastMult?: number;
  seed?: number;
}

// 移植自 collision/include/scenario.h 的 scenario::spatialCluster。
// C++ 版座標以原點為中心 [-box/2, box/2]，這裡的模擬空間是 [0, bounds]，因此最後平移 bounds/2。
export function spatialCluster({ count, bounds, radius, speed, acc, clusterFactor, hotspotSpread, hotspotCount, fastRatio = 0, fastMult = 10, seed = 124 }: SpatialClusterOptions): ParticleData[] {
  if (hotspotCount <= 0) throw new Error('hotspot number cannot be negative.');
  const random = () => { seed = (seed * 1664525 + 1013904223) >>> 0; return seed / 4294967296; };
  const uniform = (min: number, max: number) => min + random() * (max - min);
  // Box-Muller，對應 std::normal_distribution
  const normal = (stddev: number) => { const u = 1 - random(); const v = random(); return Math.sqrt(-2 * Math.log(u)) * Math.cos(2 * Math.PI * v) * stddev; };

  const axes = ['x', 'y', 'z'] as const;
  const hotspots: Vec3[] = Array.from({ length: hotspotCount }, () => {
    const h = { x: 0, y: 0, z: 0 };
    axes.forEach((axis) => { h[axis] = uniform(-bounds[axis] * 0.4, bounds[axis] * 0.4); });
    return h;
  });

  return Array.from({ length: count }, (_, id) => {
    const position = { x: 0, y: 0, z: 0 };
    if (random() < clusterFactor) {
      const center = hotspots[Math.min(hotspotCount - 1, Math.floor(random() * hotspotCount))];
      axes.forEach((axis) => { position[axis] = center[axis] + normal(hotspotSpread); });
    } else {
      axes.forEach((axis) => { position[axis] = uniform(-bounds[axis] * 0.5, bounds[axis] * 0.5); });
    }
    axes.forEach((axis) => {
      const half = bounds[axis] * 0.5;
      position[axis] = Math.min(Math.max(position[axis], -half + radius), half - radius) + half;
    });

    const mult = random() < fastRatio ? fastMult : 1;
    const velocity = { x: uniform(-speed, speed) * mult, y: uniform(-speed, speed) * mult, z: uniform(-speed, speed) * mult };
    const acceleration = { x: uniform(-acc, acc) * mult, y: uniform(-acc, acc) * mult, z: uniform(-acc, acc) * mult };
    return { id, position, velocity, acceleration, radius, mass: 1, positionAtLastBroadPhase: { ...position }, skin: 0 };
  });
}
