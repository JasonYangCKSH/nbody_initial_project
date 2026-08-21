export type Vec3 = { x: number; y: number; z: number };

export interface ParticleData {
  id: number;
  position: Vec3;
  velocity: Vec3;
  radius: number;
  positionAtLastBroadPhase: Vec3;
  skin: number;
}

export interface DebugGeometry {
  kind: 'grid';
  cellSize: number;
  bounds: { x: number; y: number; z: number };
}

export interface SpatialStructure {
  build(particles: ParticleData[]): void;
  queryCandidatePairs(withSkin?: boolean): [number, number][];
  getDebugGeometry(): DebugGeometry;
  getMetrics(): { distanceChecks: number; candidatePairs: number };
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
