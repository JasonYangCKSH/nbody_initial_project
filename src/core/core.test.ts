import { describe, expect, it } from 'vitest';
import { BruteForceStructure } from './spatial/BruteForceStructure';
import { OctreeStructure } from './spatial/OctreeStructure';
import { UniformGridStructure } from './spatial/UniformGridStructure';
import { VerletBufferController } from './VerletBufferController';
import { ParticleSystem } from './ParticleSystem';
import { shellCenter, shellCoreRadius, shellOf } from './scenario';
import { BLOCKER_RADIUS, REACH, blockSpot, blocksLine, guardPosts, inReach, stepToward, viewClarity, type Blocker } from './blockers';
import type { ParticleData } from './types';

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
  it('blocker game: a tall blocker on the line of sight blocks it, a fallen or distant one does not', () => {
    const groundY = -12.5;
    const eye = { x: 0, y: groundY + 1.7, z: 17 };
    const firework = { x: 0, y: 5, z: 0 };
    const near: Blocker = { id: 0, x: 0, z: 14, state: 'standing', timer: 0, slot: 0 };
    expect(blocksLine(eye, firework, near, groundY)).toBe(true);
    // 往旁邊移開超過半徑就不擋
    expect(blocksLine(eye, firework, { x: BLOCKER_RADIUS + 0.2, z: 14 }, groundY)).toBe(false);
    // 太靠近煙火時視線已經比阻擋者高
    expect(blocksLine(eye, firework, { x: 0, z: 1 }, groundY)).toBe(false);
    // 站在玩家背後不擋
    expect(blocksLine(eye, firework, { x: 0, z: 20 }, groundY)).toBe(false);
    const targets = [firework, { x: 6, y: 6, z: 0 }];
    expect(viewClarity(eye, targets, [near], groundY)).toBe(0.5);
    expect(viewClarity(eye, targets, [{ ...near, state: 'down' }], groundY)).toBe(1);
  });
  it('blocker game: blockSpot puts slot 0 on the line of sight and stepToward never overshoots', () => {
    const groundY = -12.5;
    const player = { x: 0, z: 17 };
    const spot = blockSpot(player, { x: 0, z: 0 }, 0);
    expect(spot.x).toBeCloseTo(0);
    expect(spot.z).toBeLessThan(player.z);
    expect(blocksLine({ x: 0, y: groundY + 1.7, z: 17 }, { x: 0, y: 5, z: 0 }, spot, groundY)).toBe(true);
    expect(stepToward({ x: 0, z: 0 }, { x: 3, z: 4 }, 1)).toEqual({ x: 0.6, z: 0.8 });
    expect(stepToward({ x: 0, z: 0 }, { x: 0.3, z: 0.4 }, 1)).toEqual({ x: 0.3, z: 0.4 });
  });
  it('blocker game: guards posted in front of the best spot block every firework until knocked down', () => {
    const groundY = -12.5;
    const spot = { x: -3, z: 13.5 };
    const eye = { x: spot.x, y: groundY + 1.7, z: spot.z };
    for (const shells of [1, 3, 8]) {
      const targets = Array.from({ length: shells }, (_, i) => ({ x: -8 + i * 2.5, y: 3 + (i % 3) * 2, z: -2 + (i % 2) * 3 }));
      const count = Math.min(8, Math.max(6, shells));
      const guards: Blocker[] = guardPosts(spot, targets, count).map((p, id) => ({ id, ...p, state: 'standing', timer: 0, slot: id }));
      expect(viewClarity(eye, targets, guards, groundY)).toBe(0);
      // 每個守衛都在最佳位置的攻擊範圍內
      guards.forEach((g) => expect(inReach(spot, g)).toBe(true));
      expect(viewClarity(eye, targets, guards.map((g) => ({ ...g, state: 'down' as const })), groundY)).toBe(1);
    }
    expect(inReach({ x: 0, z: 0 }, { x: REACH + 0.1, z: 0 })).toBe(false);
  });
  it('octree getLeafDepths matches getLeafOf for every particle', () => {
    const particles = [particle(0, 0.2), particle(1, 0.35), particle(2, 3), particle(3, 1.5), particle(4, 1.55)];
    const tree = new OctreeStructure({ x: 4, y: 4, z: 4 }, 2, 2); tree.build(particles);
    expect(tree.getLeafDepths()).toEqual(particles.map((_, index) => tree.getLeafOf(index)!.depth));
    expect(new OctreeStructure({ x: 4, y: 4, z: 4 }).getLeafDepths()).toEqual([]);
  });
});
