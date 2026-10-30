// =============================================================================
// 讓觀眾「看得見」的效果：候選配對上色與連線、碰撞擴散光環、焦點球光圈
// =============================================================================
import { useFrame, useThree } from '@react-three/fiber';
import { Billboard } from '@react-three/drei';
import { useEffect, useMemo, useRef } from 'react';
import * as THREE from 'three';
import { candidateGroups } from '../core/candidateGroups';
import type { ParticleData, Vec3 } from '../core/types';
import { toScene } from './scene';

// 候選配對的固定色盤（10 色），避開碰撞的橘紅、觸發的紫色、選取的薄荷綠，也和未配對的灰色有明顯差別
export const PAIR_PALETTE = ['#4DA3FF', '#FFD84D', '#FF5FA2', '#9EE548', '#3FD8F2', '#FFA640', '#E070FF', '#4FD16B', '#7C8CFF', '#C8A27A'];
export const FOCUS_COLOR = '#FFF3B0';
const BURST_COLOR = '#FFB27A';

// 每顆粒子的候選配對顏色（不在任何配對裡就是 null）。
// 相連的配對算同一群、同一個顏色；附近（距離 near 以內）的群組盡量挑不同顏色，觀眾「在附近找同色」才不會找錯。
// offset 每次重建都不同，所以重建時顏色會整批換掉，這本身就是「名單重新產生了」的訊號
export function pairColorsFor(pairs: [number, number][], particles: ParticleData[], near: number, offset: number): (THREE.Color | null)[] {
  const { groupOf, groupCount } = candidateGroups(pairs, particles.length);
  // 每群的中心位置
  const center = Array.from({ length: groupCount }, () => ({ x: 0, y: 0, z: 0, n: 0 }));
  groupOf.forEach((group, index) => {
    if (group < 0) return;
    const c = center[group]; const p = particles[index].position;
    c.x += p.x; c.y += p.y; c.z += p.z; c.n += 1;
  });
  center.forEach((c) => { c.x /= c.n; c.y /= c.n; c.z /= c.n; });
  // 用邊長 near 的格子記錄已上色的群組，只檢查周圍 27 格，群組很多時也很快
  const cells = new Map<string, number[]>();
  const cellOf = (v: number) => Math.floor(v / near);
  const colorOf = new Int32Array(groupCount);
  center.forEach((c, group) => {
    const used = new Set<number>();
    const [cx, cy, cz] = [cellOf(c.x), cellOf(c.y), cellOf(c.z)];
    for (let dx = -1; dx <= 1; dx += 1) for (let dy = -1; dy <= 1; dy += 1) for (let dz = -1; dz <= 1; dz += 1) {
      cells.get(`${cx + dx}:${cy + dy}:${cz + dz}`)?.forEach((other) => {
        const o = center[other];
        if (Math.hypot(o.x - c.x, o.y - c.y, o.z - c.z) < near) used.add(colorOf[other]);
      });
    }
    // 每群從不同的顏色開始找（群組編號 × 7，7 和 10 互質，會輪過全部 10 色），附近已用過的顏色就跳過
    const start = (offset + group * 7) % PAIR_PALETTE.length;
    let color = start;
    for (let k = 0; k < PAIR_PALETTE.length; k += 1) {
      const candidate = (start + k) % PAIR_PALETTE.length;
      if (!used.has(candidate)) { color = candidate; break; }
    }
    colorOf[group] = color;
    const key = `${cx}:${cy}:${cz}`;
    cells.set(key, [...(cells.get(key) ?? []), group]);
  });
  const palette = PAIR_PALETTE.map((hex) => new THREE.Color(hex));
  return Array.from(groupOf, (group) => (group < 0 ? null : palette[colorOf[group]]));
}

// 候選配對連線：每一組候選配對畫一條細線，顏色和那一群相同。名單只在重建時改變，線的兩端跟著球移動
export function PairLines({ pairs, particles, bounds, pairColors }: { pairs: [number, number][]; particles: ParticleData[]; bounds: Vec3; pairColors: (THREE.Color | null)[] }) {
  const geometry = useMemo(() => {
    const geom = new THREE.BufferGeometry();
    geom.setAttribute('position', new THREE.Float32BufferAttribute(new Float32Array(pairs.length * 6), 3));
    const colors = new Float32Array(pairs.length * 6);
    pairs.forEach(([a], i) => { const c = pairColors[a]; if (c) colors.set([c.r, c.g, c.b, c.r, c.g, c.b], i * 6); });
    geom.setAttribute('color', new THREE.Float32BufferAttribute(colors, 3));
    return geom;
  }, [pairs, pairColors]);
  useEffect(() => () => geometry.dispose(), [geometry]);
  useFrame(() => {
    const position = geometry.getAttribute('position') as THREE.BufferAttribute;
    const array = position.array as Float32Array;
    const hx = bounds.x / 2, hy = bounds.y / 2, hz = bounds.z / 2;
    pairs.forEach(([a, b], i) => {
      const p = particles[a].position, q = particles[b].position;
      array.set([p.x - hx, p.y - hy, p.z - hz, q.x - hx, q.y - hy, q.z - hz], i * 6);
    });
    position.needsUpdate = true;
    geometry.computeBoundingSphere();
  });
  return <lineSegments geometry={geometry} raycast={() => null}>
    <lineBasicMaterial vertexColors transparent opacity={0.75} toneMapped={false} />
  </lineSegments>;
}

// 碰撞擴散光環：新碰撞發生時，在兩顆球的接觸點放一個向外擴大、逐漸變淡的光環（加法混色，變黑就等於透明）
const BURST_MS = 800;
const BURST_POOL = 24;
export function CollisionBursts({ pairs, stepId, particles, bounds }: { pairs: [number, number][]; stepId: number; particles: ParticleData[]; bounds: Vec3 }) {
  const mesh = useRef<THREE.InstancedMesh>(null);
  const bursts = useRef(Array.from({ length: BURST_POOL }, () => ({ x: 0, y: 0, z: 0, size: 0, start: -Infinity })));
  const next = useRef(0);
  const dummy = useRef(new THREE.Object3D());
  const color = useRef(new THREE.Color());
  const base = useMemo(() => new THREE.Color(BURST_COLOR), []);
  const camera = useThree((state) => state.camera);
  useEffect(() => {
    const now = performance.now();
    pairs.forEach(([a, b]) => {
      const p = particles[a], q = particles[b];
      // 接觸點：兩顆球心連線上、離 a 球心 r_a 的位置
      const t = p.radius / (p.radius + q.radius);
      const slot = bursts.current[next.current];
      next.current = (next.current + 1) % BURST_POOL;
      Object.assign(slot, { x: p.position.x + (q.position.x - p.position.x) * t, y: p.position.y + (q.position.y - p.position.y) * t, z: p.position.z + (q.position.z - p.position.z) * t, size: Math.max(p.radius, q.radius), start: now });
    });
  }, [stepId]); // eslint-disable-line react-hooks/exhaustive-deps
  useFrame(() => {
    if (!mesh.current) return;
    const now = performance.now();
    bursts.current.forEach((burst, index) => {
      const t = (now - burst.start) / BURST_MS;
      const alive = t >= 0 && t < 1;
      dummy.current.position.set(burst.x - bounds.x / 2, burst.y - bounds.y / 2, burst.z - bounds.z / 2);
      dummy.current.quaternion.copy(camera.quaternion);
      dummy.current.scale.setScalar(alive ? burst.size * (1.2 + 2.8 * t) : 0);
      dummy.current.updateMatrix();
      mesh.current!.setMatrixAt(index, dummy.current.matrix);
      mesh.current!.setColorAt(index, color.current.copy(base).multiplyScalar(alive ? (1 - t) ** 2 : 0));
    });
    mesh.current.instanceMatrix.needsUpdate = true;
    if (mesh.current.instanceColor) mesh.current.instanceColor.needsUpdate = true;
  });
  return <instancedMesh ref={mesh} args={[undefined, undefined, BURST_POOL]} raycast={() => null} frustumCulled={false}>
    <ringGeometry args={[0.8, 1, 48]} />
    <meshBasicMaterial transparent blending={THREE.AdditiveBlending} depthWrite={false} toneMapped={false} side={THREE.DoubleSide} />
  </instancedMesh>;
}

// 焦點球光圈：一律面向鏡頭、緩慢呼吸閃爍的圓環，讓觀眾一眼找到安排好會相撞的那兩顆
export function FocusHalos({ ids, particles, bounds }: { ids: number[]; particles: ParticleData[]; bounds: Vec3 }) {
  return <>{ids.map((id) => particles[id] && <FocusHalo key={id} particle={particles[id]} bounds={bounds} />)}</>;
}

function FocusHalo({ particle, bounds }: { particle: ParticleData; bounds: Vec3 }) {
  const group = useRef<THREE.Group>(null);
  const material = useRef<THREE.MeshBasicMaterial>(null);
  useFrame(({ clock }) => {
    group.current?.position.copy(toScene(particle.position, bounds));
    if (material.current) material.current.opacity = 0.55 + 0.35 * Math.sin(clock.elapsedTime * 3);
  });
  const inner = particle.radius * 1.3;
  return <group ref={group}>
    <Billboard>
      <mesh raycast={() => null}><ringGeometry args={[inner, inner * 1.16, 48]} /><meshBasicMaterial ref={material} color={FOCUS_COLOR} transparent depthTest={false} toneMapped={false} /></mesh>
    </Billboard>
  </group>;
}
