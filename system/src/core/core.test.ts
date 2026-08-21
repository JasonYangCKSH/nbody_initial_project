import { describe, expect, it } from 'vitest';
import { BruteForceStructure } from './spatial/BruteForceStructure';
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
  it('caps skin at half cell size minus radius', () => {
    const particles = [particle(0, 0)];
    const controller = new VerletBufferController(0.15, 100, 1);
    controller.updateSkins(particles, 0.8);
    expect(particles[0].skin).toBeCloseTo(0.3);
  });
});
