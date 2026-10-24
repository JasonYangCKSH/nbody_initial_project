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

export interface ExplosionOptions {
  count: number;
  bounds: Vec3;
  radius: number;
  // 慢速粒子的基準速度；快速火花為其 2～3 倍
  burstSpeed: number;
  gravity: number;
  // 快速火花所佔比例
  sparkRatio?: number;
  // 煙火發數（同時炸開幾發）
  shellCount?: number;
  seed?: number;
}

export const MAX_SHELLS = 8;
// 前 3 發用固定、好看的位置；更多發時，其餘的爆炸中心以固定亂數散佈在空中（以模擬空間的比例表示，0～1）
const PRESET_SHELLS: Vec3[] = [
  { x: 0.3, y: 0.7, z: 0.45 },
  { x: 0.56, y: 0.6, z: 0.58 },
  { x: 0.74, y: 0.78, z: 0.4 },
];
export function shellFractions(shellCount: number): Vec3[] {
  let seed = 99;
  const random = () => { seed = (seed * 1664525 + 1013904223) >>> 0; return seed / 4294967296; };
  return Array.from({ length: shellCount }, (_, i) => PRESET_SHELLS[i] ?? { x: 0.2 + random() * 0.6, y: 0.55 + random() * 0.27, z: 0.3 + random() * 0.4 });
}
// 粒子 id % 發數 決定屬於哪一發
export const shellOf = (id: number, shellCount: number) => id % shellCount;
export const shellCenter = (shell: number, bounds: Vec3, shellCount: number): Vec3 => {
  const f = shellFractions(shellCount)[shell];
  return { x: f.x * bounds.x, y: f.y * bounds.y, z: f.z * bounds.z };
};
// 核心半徑隨每發粒子數的立方根成長，粒子數變多時初始重疊才不會太嚴重
export const shellCoreRadius = (count: number, shellCount: number) => Math.max(0.6, 0.12 * Math.cbrt(count / shellCount));

// 煙火爆炸：數發煙火同時在空中不同位置炸開。每發的粒子一開始擠在小球內，沿「從該發中心往外」的方向飛出，
// 一部分是快速火花、其餘較慢，並受向下的重力。同時展示密度異質（中心極密 → 逐漸散開）與速度異質。
export function explosion({ count, bounds, radius, burstSpeed, gravity, sparkRatio = 0.25, shellCount = 3, seed = 2026 }: ExplosionOptions): ParticleData[] {
  const random = () => { seed = (seed * 1664525 + 1013904223) >>> 0; return seed / 4294967296; };
  const uniform = (min: number, max: number) => min + random() * (max - min);
  const coreRadius = shellCoreRadius(count, shellCount);
  const centers = Array.from({ length: shellCount }, (_, shell) => shellCenter(shell, bounds, shellCount));

  return Array.from({ length: count }, (_, id) => {
    const center = centers[shellOf(id, shellCount)];
    // 球面上的均勻方向（cosθ 均勻分布）
    const cosTheta = uniform(-1, 1);
    const sinTheta = Math.sqrt(1 - cosTheta * cosTheta);
    const phi = uniform(0, 2 * Math.PI);
    const dir = { x: sinTheta * Math.cos(phi), y: cosTheta, z: sinTheta * Math.sin(phi) };
    // r = R·∛u 讓粒子在球體內均勻分布，而不是擠在球心
    const r = coreRadius * Math.cbrt(random());
    const position = { x: center.x + dir.x * r, y: center.y + dir.y * r, z: center.z + dir.z * r };
    const speed = burstSpeed * (random() < sparkRatio ? uniform(2, 3) : uniform(0.3, 1));
    const velocity = { x: dir.x * speed, y: dir.y * speed, z: dir.z * speed };
    const acceleration = { x: 0, y: -gravity, z: 0 };
    return { id, position, velocity, acceleration, radius, mass: 1, positionAtLastBroadPhase: { ...position }, skin: 0 };
  });
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
