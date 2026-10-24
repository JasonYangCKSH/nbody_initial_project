export type Vec3 = { x: number; y: number; z: number };

export interface ParticleData {
  id: number;
  position: Vec3;
  velocity: Vec3;
  // 對應 C++ Particle::acc，一律存在（預設可為零向量）
  acceleration: Vec3;
  radius: number;
  // 對應 C++ Particle::mass（預設 1）
  mass: number;
  positionAtLastBroadPhase: Vec3;
  skin: number;
}

export type DebugGeometry =
  | { kind: 'grid'; cellSize: number; bounds: Vec3 }
  // boxes：有粒子的葉節點；splits：已再往下切開的中間節點（含根節點，depth 0）
  | { kind: 'octree'; boxes: { center: Vec3; halfExtent: number; depth: number }[]; splits: { center: Vec3; halfExtent: number; depth: number }[] };

export interface SpatialStructure {
  build(particles: ParticleData[]): void;
  queryCandidatePairs(withSkin?: boolean): [number, number][];
  getDebugGeometry(): DebugGeometry;
  getMetrics(): { distanceChecks: number; candidatePairs: number };
  // Per-particle max skin (post build()); grid caps by cellSize/2 - radius, octree by leafHalfExtent - radius.
  getSkinCaps?(particles: ParticleData[]): number[];
}

export interface RebuildEvent {
  step: number;
  triggeredByParticleId: number;
  displacement: number;
  skinAtTrigger: number;
}

export interface StepMetrics {
  step: number;
  algorithm: string;
  elapsedMs: number;
  broadPhaseMs: number;
  narrowPhaseMs: number;
  distanceChecks: number;
  candidatePairs: number;
  collisions: number;
  rebuilt: boolean;
  rebuildCount: number;
  skippedSteps: number;
}

export interface LogEntry extends RebuildEvent {
  message: string;
}

export const add = (a: Vec3, b: Vec3): Vec3 => ({ x: a.x + b.x, y: a.y + b.y, z: a.z + b.z });
export const scale = (a: Vec3, factor: number): Vec3 => ({ x: a.x * factor, y: a.y * factor, z: a.z * factor });
export const distance = (a: Vec3, b: Vec3): number => Math.hypot(a.x - b.x, a.y - b.y, a.z - b.z);
