import { describe, expect, it } from 'vitest';
import { BruteForceStructure } from './spatial/BruteForceStructure';
import { OctreeStructure } from './spatial/OctreeStructure';
import { UniformGridStructure } from './spatial/UniformGridStructure';
import { VerletBufferController } from './VerletBufferController';
import { ParticleSystem } from './ParticleSystem';
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
});
