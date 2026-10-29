import { describe, expect, it } from 'vitest';
import { BruteForceStructure } from './spatial/BruteForceStructure';
import { OctreeStructure } from './spatial/OctreeStructure';
import { UniformGridStructure } from './spatial/UniformGridStructure';
import { VerletBufferController } from './VerletBufferController';
import { ParticleSystem } from './ParticleSystem';
import { shellCenter, shellCoreRadius, shellOf } from './scenario';
import { cellSizeFor, SimulationRunner, type SimConfig } from './SimulationRunner';
import { candidateGroups } from './candidateGroups';
import { MICRO_SCRIPT } from '../presets';
import type { ParticleData } from './types';

// 暴力法算出的「真正碰撞」配對（和 narrow-phase 同一個判斷式），編號為 a × 粒子數 + b
const bruteCollisionKeys = (particles: ParticleData[]) => {
  const keys: number[] = [];
  particles.forEach((p, a) => particles.slice(a + 1).forEach((q, offset) => {
    const r = p.radius + q.radius;
    if (r * r - ((p.position.x - q.position.x) ** 2 + (p.position.y - q.position.y) ** 2 + (p.position.z - q.position.z) ** 2) >= 0) keys.push(a * particles.length + a + 1 + offset);
  }));
  return keys;
};
// 邊長 6 的空間最多切 3 層：最小葉節點半邊長 0.375 仍大於半徑 0.15，skin 上界不會變成 0（變成 0 就每步都重建）
const runnerConfig = (overrides: Partial<SimConfig>): SimConfig => ({ scene: 'cloud', count: 400, worldSize: 6, radius: 0.15, speed: 0.75, clusterFactor: 0.5, clusters: 2, acceleration: 0.5, seed: 17, algorithm: 'Uniform Grid', bufferEnabled: true, K: 30, leafCapacity: 8, maxDepth: 3, ...overrides });

const particle = (id: number, x: number): ParticleData => ({ id, position: { x, y: 0, z: 0 }, velocity: { x: 1, y: 0, z: 0 }, acceleration: { x: 0, y: 0, z: 0 }, radius: 0.1, mass: 1, positionAtLastBroadPhase: { x, y: 0, z: 0 }, skin: 0.2 });
describe('collision core', () => {
  it('keeps uniform grid candidate pairs equivalent to brute force', () => {
    const particles = [particle(0, 0.2), particle(1, 0.35), particle(2, 3)];
    const brute = new BruteForceStructure(); brute.build(particles);
    const grid = new UniformGridStructure({ x: 4, y: 1, z: 1 }, 0.8); grid.build(particles);
    expect(grid.queryCandidatePairs()).toEqual(brute.queryCandidatePairs());
  });
  it('keeps octree candidate pairs equivalent to brute force', () => {
    const particles = [
      particle(0, 0.2),
      particle(1, 0.35),
      particle(2, 3),
      particle(3, 1.5),
      particle(4, 1.55),
    ];
    const brute = new BruteForceStructure(); brute.build(particles);
    const tree = new OctreeStructure({ x: 4, y: 4, z: 4 }, 2, 2); tree.build(particles);
    expect(tree.queryCandidatePairs()).toEqual(brute.queryCandidatePairs());
  });
  it('grid caps skin at half cell size minus radius', () => {
    const particles = [particle(0, 0)];
    const grid = new UniformGridStructure({ x: 4, y: 4, z: 4 }, 0.8); grid.build(particles);
    const controller = new VerletBufferController(0.15, 100, 1);
    controller.updateSkins(particles, grid.getSkinCaps(particles));
    expect(particles[0].skin).toBeCloseTo(0.3);
  });
  it('octree caps skin at leaf half extent minus radius', () => {
    const particles = [particle(0, 0)];
    const tree = new OctreeStructure({ x: 4, y: 4, z: 4 }, 6, 8); tree.build(particles);
    const controller = new VerletBufferController(0.15, 100, 1);
    controller.updateSkins(particles, tree.getSkinCaps(particles));
    expect(particles[0].skin).toBeCloseTo(1.9);
  });
  it('supports brute-force candidate filtering with verlet skin', () => {
    const particles = [
      { ...particle(0, 0), skin: 0.2 },
      { ...particle(1, 0.29), skin: 0.2 },
    ];
    const brute = new BruteForceStructure(); brute.build(particles);
    expect(brute.queryCandidatePairs(true)).toEqual([[0, 1]]);
  });
  it('includes acceleration in skin: K·|v|·Δt + ½·|a|·(K·Δt)²', () => {
    const p = { ...particle(0, 0), velocity: { x: 3, y: 4, z: 0 }, acceleration: { x: 0, y: 0, z: 2 } };
    const controller = new VerletBufferController(0.15, 10, 0.1);
    expect(controller.computeSkin(p)).toBeCloseTo(5 * 1 + 0.5 * 2 * 1 * 1);
  });
  it('integrates acceleration before position (semi-implicit Euler, same as C++ integrate)', () => {
    const system = new ParticleSystem(1, { x: 10, y: 10, z: 10 }, 0, 0);
    const p = system.particles[0];
    p.position = { x: 5, y: 5, z: 5 }; p.velocity = { x: 1, y: 0, z: 0 }; p.acceleration = { x: 2, y: 0, z: 0 };
    system.step(0.5);
    expect(p.velocity.x).toBeCloseTo(2);
    expect(p.position.x).toBeCloseTo(6);
  });
  it('reflects off walls by negating velocity (same as C++ reflectOffWalls)', () => {
    const system = new ParticleSystem(1, { x: 10, y: 10, z: 10 }, 0, 0);
    const p = system.particles[0];
    p.position = { x: 9.95, y: 0.05, z: 5 }; p.velocity = { x: 1, y: -2, z: 0 };
    system.step(0.1);
    expect(p.position.x).toBeCloseTo(10 - p.radius); expect(p.velocity.x).toBe(-1);
    expect(p.position.y).toBeCloseTo(p.radius); expect(p.velocity.y).toBe(2);
  });
  it('resolves collisions with mass-weighted elastic impulse (same as C++ resolveCollisions)', () => {
    const system = new ParticleSystem(2, { x: 10, y: 10, z: 10 }, 0, 0);
    const [a, b] = system.particles;
    Object.assign(a, { position: { x: 5, y: 5, z: 5 }, velocity: { x: 1, y: 0, z: 0 }, mass: 1 });
    Object.assign(b, { position: { x: 5.1, y: 5, z: 5 }, velocity: { x: -1, y: 0, z: 0 }, mass: 3 });
    system.resolveCollisions([[0, 1]]);
    // J = -2·2 / (1 + 1/3) = -3 → a.v = 1 - 3 = -2，b.v = -1 + 3/3 = 0（動量守恆：1·1 + 3·(-1) = 1·(-2) + 3·0）
    expect(a.velocity.x).toBeCloseTo(-2); expect(b.velocity.x).toBeCloseTo(0);
    // 重疊 0.05 依質量反比推開：a 移 0.0375，b 移 0.0125
    expect(a.position.x).toBeCloseTo(4.9625); expect(b.position.x).toBeCloseTo(5.1125);
  });
  it('skips response for separating pairs', () => {
    const system = new ParticleSystem(2, { x: 10, y: 10, z: 10 }, 0, 0);
    const [a, b] = system.particles;
    Object.assign(a, { position: { x: 5, y: 5, z: 5 }, velocity: { x: -1, y: 0, z: 0 } });
    Object.assign(b, { position: { x: 5.1, y: 5, z: 5 }, velocity: { x: 1, y: 0, z: 0 } });
    system.resolveCollisions([[0, 1]]);
    expect(a.velocity.x).toBe(-1); expect(a.position.x).toBe(5);
  });
  it('scenario acceleration components stay within [-acc, acc]', () => {
    const acc = 0.8;
    const { particles } = new ParticleSystem(500, { x: 10, y: 10, z: 10 }, 0, acc);
    particles.forEach(({ acceleration }) => (['x', 'y', 'z'] as const).forEach((axis) => expect(Math.abs(acceleration[axis])).toBeLessThanOrEqual(acc)));
    expect(new ParticleSystem(10, { x: 10, y: 10, z: 10 }, 0, 0).particles.every(({ acceleration }) => acceleration.x === 0 && acceleration.y === 0 && acceleration.z === 0)).toBe(true);
  });
  it('octree debug geometry lists leaf boxes that contain every particle', () => {
    const particles = [particle(0, 0.2), particle(1, 0.35), particle(2, 3), particle(3, 1.5), particle(4, 1.55)];
    const tree = new OctreeStructure({ x: 4, y: 4, z: 4 }, 2, 2); tree.build(particles);
    const geometry = tree.getDebugGeometry();
    expect(geometry.kind).toBe('octree');
    if (geometry.kind !== 'octree') return;
    geometry.boxes.forEach(({ halfExtent, depth }) => expect(halfExtent).toBeCloseTo(2 / 2 ** depth));
    // 5 顆粒子、葉容量 2：根節點必定被切開，且每個切開節點都比它的葉節點淺
    expect(geometry.splits.some(({ depth }) => depth === 0)).toBe(true);
    geometry.splits.forEach(({ halfExtent, depth }) => expect(halfExtent).toBeCloseTo(2 / 2 ** depth));
    expect(Math.max(...geometry.splits.map(({ depth }) => depth))).toBeLessThan(Math.max(...geometry.boxes.map(({ depth }) => depth)));
    particles.forEach((p) => expect(geometry.boxes.some(({ center, halfExtent }) =>
      Math.abs(p.position.x - center.x) <= halfExtent && Math.abs(p.position.y - center.y) <= halfExtent && Math.abs(p.position.z - center.z) <= halfExtent)).toBe(true));
  });
  it('octree getLeafOf returns the leaf containing the particle and its depth', () => {
    const particles = [particle(0, 0.2), particle(1, 0.35), particle(2, 3), particle(3, 1.5), particle(4, 1.55)];
    const tree = new OctreeStructure({ x: 4, y: 4, z: 4 }, 2, 2); tree.build(particles);
    particles.forEach((p, index) => {
      const leaf = tree.getLeafOf(index)!;
      expect(Math.abs(p.position.x - leaf.center.x)).toBeLessThanOrEqual(leaf.halfExtent);
      expect(leaf.halfExtent).toBeCloseTo(2 / 2 ** leaf.depth);
      // 未達最大深度的葉節點，粒子數不會超過容量
      if (leaf.depth < tree.getLimits().maxDepth) expect(leaf.count).toBeLessThanOrEqual(tree.getLimits().leafCapacity);
    });
    expect(tree.getLimits()).toEqual({ leafCapacity: 2, maxDepth: 2 });
    expect(new OctreeStructure({ x: 4, y: 4, z: 4 }).getLeafOf(0)).toBeNull();
  });
  it('explosion scene starts packed at each shell centre and flies outward with mixed speeds under gravity', () => {
    const bounds = { x: 25, y: 25, z: 25 };
    const { particles } = new ParticleSystem(2000, bounds, 0, 0.4, 17, 'explosion');
    const coreRadius = shellCoreRadius(2000, 3);
    const speeds = particles.map(({ velocity: v }) => Math.hypot(v.x, v.y, v.z));
    particles.forEach(({ id, position: p, velocity: v, acceleration: a }) => {
      const center = shellCenter(shellOf(id, 3), bounds, 3);
      const offset = { x: p.x - center.x, y: p.y - center.y, z: p.z - center.z };
      expect(Math.hypot(offset.x, offset.y, offset.z)).toBeLessThanOrEqual(coreRadius + 1e-9);
      expect(offset.x * v.x + offset.y * v.y + offset.z * v.z).toBeGreaterThanOrEqual(0); // 往外飛
      expect(a).toEqual({ x: 0, y: -0.4, z: 0 });
    });
    // 速度異質：最快的火花至少是最慢粒子的 5 倍，且快速火花約佔 25%
    expect(Math.max(...speeds) / Math.min(...speeds)).toBeGreaterThan(5);
    const sparks = speeds.filter((s) => s > 2.5 * 1.5).length / speeds.length;
    expect(sparks).toBeGreaterThan(0.18);
    expect(sparks).toBeLessThan(0.32);
  });
  it('cluster count controls the number of firework shells and cloud hotspots', () => {
    const bounds = { x: 25, y: 25, z: 25 };
    const fireworks = new ParticleSystem(1000, bounds, 0, 0.25, 17, 'explosion', 6);
    const coreRadius = shellCoreRadius(1000, 6);
    // 每顆粒子都在自己那一發的核心球內，且 6 發都有粒子
    const used = new Set<number>();
    fireworks.particles.forEach(({ id, position: p }) => {
      const shell = shellOf(id, 6); used.add(shell);
      const c = shellCenter(shell, bounds, 6);
      expect(Math.hypot(p.x - c.x, p.y - c.y, p.z - c.z)).toBeLessThanOrEqual(coreRadius + 1e-9);
    });
    expect(used.size).toBe(6);
    expect(new Set(fireworks.colors).size).toBeGreaterThanOrEqual(6);
    // 前 3 發的位置不受總發數影響
    expect(shellCenter(0, bounds, 6)).toEqual(shellCenter(0, bounds, 3));
    expect(new ParticleSystem(10, bounds, 1, 0.25, 17, 'cloud', 4).clusters).toBe(4);
  });
  it('octree getLeafDepths matches getLeafOf for every particle', () => {
    const particles = [particle(0, 0.2), particle(1, 0.35), particle(2, 3), particle(3, 1.5), particle(4, 1.55)];
    const tree = new OctreeStructure({ x: 4, y: 4, z: 4 }, 2, 2); tree.build(particles);
    expect(tree.getLeafDepths()).toEqual(particles.map((_, index) => tree.getLeafOf(index)!.depth));
    expect(new OctreeStructure({ x: 4, y: 4, z: 4 }).getLeafDepths()).toEqual([]);
  });
  it('octree keeps splitting a tight cluster down to max depth (same as C++ Octree::split)', () => {
    // 9 顆擠在 0.02 的範圍內、葉容量 8：每一層切開後 9 顆都落在同一個子格，要一路切到第 6 層
    const particles = Array.from({ length: 9 }, (_, i) => ({ ...particle(i, 0), position: { x: 3 + i * 0.002, y: 3 + i * 0.001, z: 3 } }));
    const tree = new OctreeStructure({ x: 25, y: 25, z: 25 }, 6, 8); tree.build(particles);
    expect(tree.getLeafOf(0)).toMatchObject({ depth: 6, count: 9 });
  });
  it('runner with Verlet buffer finds exactly the brute-force collisions every step (grid and octree)', () => {
    for (const algorithm of ['Uniform Grid', 'Octree'] as const) {
      const runner = new SimulationRunner(runnerConfig({ algorithm }));
      // 每 5 顆有 1 顆速度放大 6 倍：每顆粒子的 skin 差很多，也會被撞到改變方向
      runner.particles.forEach((p, i) => { if (i % 5 === 0) p.velocity = { x: p.velocity.x * 6, y: p.velocity.y * 6, z: p.velocity.z * 6 }; });
      let collisions = 0;
      for (let s = 0; s < 150; s += 1) {
        const truth = bruteCollisionKeys(runner.particles);
        runner.step();
        expect(runner.collisionPairs.map(([a, b]) => a * runner.particles.length + b)).toEqual(truth);
        collisions += truth.length;
      }
      expect(collisions).toBeGreaterThan(50);
      expect(runner.metrics.rebuildCount).toBeLessThan(120); // 有不少步數沿用列表、跳過 broad-phase
    }
  });
  it('runner counts only narrow-phase checks on steps that reuse the candidate list', () => {
    // 稀疏、一開始不重疊的粒子雲：第 2 步一定沿用第 1 步的列表
    const runner = new SimulationRunner(runnerConfig({ algorithm: 'Octree', count: 100, radius: 0.05, clusterFactor: 0 }));
    const first = runner.step();
    expect(first.rebuilt).toBe(true);
    expect(first.broadPhaseChecks).toBeGreaterThan(0);
    expect(first.distanceChecks).toBe(first.broadPhaseChecks + first.candidatePairs);
    const reused = runner.step();
    expect(reused.rebuilt).toBe(false);
    expect(reused.broadPhaseChecks).toBe(0);
    expect(reused.distanceChecks).toBe(reused.candidatePairs);
    expect(reused.detectionMs).toBeCloseTo(reused.checkMs + reused.broadPhaseMs + reused.narrowPhaseMs);
    // 調整 K 之後下一步直接重建
    runner.setK(5);
    expect(runner.step().rebuilt).toBe(true);
  });
  it('grid cell size follows the particle radius (cellSizeRatio 2 × diameter)', () => {
    expect(cellSizeFor(0.075)).toBeCloseTo(0.3);
    expect(new SimulationRunner(runnerConfig({ radius: 0.35 })).cellSize).toBeCloseTo(1.4);
  });
  it('micro scene places balls inside the box without overlap, moving at constant velocity', () => {
    const bounds = { x: 8, y: 8, z: 8 };
    const { particles } = new ParticleSystem(40, bounds, 0, 0.5, 7, 'micro', 1, { radius: 0.35, speed: 1.5 });
    expect(particles).toHaveLength(40);
    particles.forEach(({ position: p, acceleration: a }, i) => {
      (['x', 'y', 'z'] as const).forEach((axis) => { expect(p[axis]).toBeGreaterThanOrEqual(0.35); expect(p[axis]).toBeLessThanOrEqual(8 - 0.35); });
      expect(a).toEqual({ x: 0, y: 0, z: 0 });
      particles.slice(i + 1).forEach(({ position: q }) => expect(Math.hypot(p.x - q.x, p.y - q.y, p.z - q.z)).toBeGreaterThan(0.7));
    });
  });
  it('script 1: focus balls become a candidate pair first, then collide with each other before touching any other ball', () => {
    const runner = new SimulationRunner(MICRO_SCRIPT.config);
    expect(runner.system.focusIds).toEqual([0, 1]);
    // 配對一律「小編號在前」，所以只要看第一個編號是不是 0 或 1
    let listedAt = 0;
    let first: [number, number] | undefined;
    for (let s = 1; s <= 120 && !first; s += 1) {
      runner.step();
      if (!listedAt && runner.pairs.some(([a, b]) => a === 0 && b === 1)) listedAt = s;
      first = runner.collisionPairs.find(([a]) => a <= 1);
    }
    expect(first).toEqual([0, 1]);
    // 相撞前至少有幾步已經是同色的候選配對，觀眾來得及看到
    expect(runner.metrics.step - listedAt).toBeGreaterThanOrEqual(5);
  });
  it('steps back to earlier frames and then replays them exactly (previous-frame button)', () => {
    const runner = new SimulationRunner(runnerConfig({ algorithm: 'Octree' }));
    runner.keepHistory(50);
    const positions = () => runner.particles.map(({ position: p }) => [p.x, p.y, p.z]);
    for (let s = 0; s < 20; s += 1) runner.step();
    const at20 = { positions: positions(), metrics: runner.metrics, pairs: runner.pairs };
    for (let s = 0; s < 10; s += 1) runner.step();
    const at30 = { positions: positions(), rebuilds: runner.metrics.rebuildCount, pairs: runner.pairs.length, history: runner.instrumentation.getHistory().length };
    for (let s = 0; s < 10; s += 1) expect(runner.stepBack()).toBe(true);
    expect(positions()).toEqual(at20.positions);
    expect(runner.metrics).toBe(at20.metrics);
    expect(runner.pairs).toBe(at20.pairs);
    expect(runner.instrumentation.getHistory().length).toBe(20);
    for (let s = 0; s < 10; s += 1) runner.step();
    expect(positions()).toEqual(at30.positions);
    expect(runner.metrics.rebuildCount).toBe(at30.rebuilds);
    expect(runner.pairs.length).toBe(at30.pairs);
    expect(runner.instrumentation.getHistory().length).toBe(at30.history);
    // 最多只保存 50 步：退到最早一步後就不能再退
    for (let s = 0; s < 30; s += 1) runner.stepBack();
    expect(runner.canStepBack).toBe(false);
    expect(runner.metrics.step).toBe(0);
  });
  it('groups candidate pairs into connected sets, numbered by their smallest particle', () => {
    const { groupOf, groupCount } = candidateGroups([[7, 8], [1, 2], [2, 5]], 10);
    expect(groupCount).toBe(2);
    expect([...groupOf]).toEqual([-1, 0, 0, -1, -1, 0, -1, 1, 1, -1]);
  });
});
