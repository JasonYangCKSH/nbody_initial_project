import { describe, expect, it } from 'vitest';
import { BruteForceStructure } from './spatial/BruteForceStructure';
import { OctreeStructure } from './spatial/OctreeStructure';
import { UniformGridStructure } from './spatial/UniformGridStructure';
import { VerletBufferController } from './VerletBufferController';
import type { ParticleData } from './types';

const particle = (id: number, x: number): ParticleData => ({ id, position: { x, y: 0, z: 0 }, velocity: { x: 1, y: 0, z: 0 }, radius: 0.1, positionAtLastBroadPhase: { x, y: 0, z: 0 }, skin: 0.2 });
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
});
