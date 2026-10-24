import { useFrame, useThree } from '@react-three/fiber';
import { useEffect, useRef } from 'react';
import * as THREE from 'three';
import { BLOCKER_HEIGHT, BLOCKER_RADIUS, guardPosts, inReach, viewClarity, type Blocker } from './core/blockers';
import type { Vec3 } from './core/types';

// 阻擋者小遊戲：守衛一開始就站在最佳觀賞位置前方、擋住每一發煙火，而且不會移動。
// 觀眾要走到攻擊範圍內點擊才能打倒；打倒後一直躺著，按「重新開始」才會再站起來。
// 站在金色光圈（最佳觀賞位置）且沒有人擋住時，累計最佳觀賞時間。
export const BEST_SPOT = { x: -3, z: 13.5 };
const BEST_SPOT_RADIUS = 1.4;
const FALL_SECONDS = 0.7;
const TOO_FAR_SECONDS = 1.5;
// 模型以約 4.6 高設計，整體放大到 BLOCKER_HEIGHT
const MODEL_SCALE = BLOCKER_HEIGHT / 4.6;
const JACKETS = ['#9a3446', '#3d55a0', '#7a5430', '#2f7a64', '#8a46a0', '#6a6a6a', '#a0702f', '#2f5a7a'];

export type GameStats = { knocked: number; remaining: number; clarity: number; atBestSpot: boolean; bestSeconds: number; tooFar: boolean };
type Spectator = { x: number; z: number };

// 滑鼠鎖定（第一人稱）時游標固定在畫面中央，射線一律從準心發出；否則用游標位置
export function pointerNdc(event: PointerEvent, canvas: HTMLCanvasElement): THREE.Vector2 {
  if (document.pointerLockElement === canvas) return new THREE.Vector2(0, 0);
  const rect = canvas.getBoundingClientRect();
  return new THREE.Vector2(((event.clientX - rect.left) / rect.width) * 2 - 1, -((event.clientY - rect.top) / rect.height) * 2 + 1);
}

export function BlockerGame({ spectator, groundY, eyeHeight, targets, restartToken, onStats }: { spectator: Spectator; groundY: number; eyeHeight: number; targets: Vec3[]; restartToken: number; onStats: (stats: GameStats) => void }) {
  const camera = useThree((state) => state.camera);
  const canvas = useThree((state) => state.gl.domElement);
  const guardCount = Math.min(JACKETS.length, Math.max(6, targets.length));
  const blockers = useRef<Blocker[]>([]);
  const groups = useRef<(THREE.Group | null)[]>([]);
  const bodies = useRef<(THREE.Group | null)[]>([]);
  const colliders = useRef<(THREE.Mesh | null)[]>([]);
  const stats = useRef<GameStats>({ knocked: 0, remaining: guardCount, clarity: 1, atBestSpot: false, bestSeconds: 0, tooFar: false });
  const tooFarTimer = useRef(0);
  const onStatsRef = useRef(onStats);
  onStatsRef.current = onStats;

  // 開始或重新開始：所有守衛回到崗位站好、計分歸零；煙火發數改變時崗位也跟著重排
  useEffect(() => {
    blockers.current = guardPosts(BEST_SPOT, targets, guardCount).map((post, id) => ({ id, x: post.x, z: post.z, state: 'standing', timer: 0, slot: id }));
    stats.current = { knocked: 0, remaining: guardCount, clarity: 1, atBestSpot: false, bestSeconds: 0, tooFar: false };
    tooFarTimer.current = 0;
  }, [restartToken, targets, guardCount]);

  useEffect(() => {
    const timer = window.setInterval(() => onStatsRef.current({ ...stats.current }), 200);
    return () => window.clearInterval(timer);
  }, []);

  // 左鍵攻擊：在捕獲階段先判斷，打中守衛就攔下事件，不會同時選到後面的粒子
  useEffect(() => {
    const raycaster = new THREE.Raycaster();
    let start: { x: number; y: number } | null = null;
    const down = (event: PointerEvent) => { start = event.button === 0 ? { x: event.clientX, y: event.clientY } : null; };
    const up = (event: PointerEvent) => {
      const from = start; start = null;
      if (!from || Math.hypot(event.clientX - from.x, event.clientY - from.y) > 5) return;
      raycaster.setFromCamera(pointerNdc(event, canvas), camera);
      const hit = raycaster.intersectObjects(colliders.current.filter((m): m is THREE.Mesh => !!m), false)[0];
      const blocker = hit ? blockers.current[hit.object.userData.blockerId as number] : undefined;
      if (!blocker || blocker.state !== 'standing') return;
      event.stopImmediatePropagation();
      if (!inReach(spectator, blocker)) { tooFarTimer.current = TOO_FAR_SECONDS; return; }
      blocker.state = 'falling'; blocker.timer = 0;
      stats.current.knocked += 1;
    };
    canvas.addEventListener('pointerdown', down);
    window.addEventListener('pointerup', up, true);
    return () => { canvas.removeEventListener('pointerdown', down); window.removeEventListener('pointerup', up, true); };
  }, [camera, canvas, spectator]);

  useFrame((_, delta) => {
    const dt = Math.min(delta, 0.25);
    blockers.current.forEach((b, i) => {
      if (b.state === 'falling') {
        b.timer += dt;
        if (b.timer >= FALL_SECONDS) { b.state = 'down'; b.timer = 0; }
      }
      const group = groups.current[i], body = bodies.current[i];
      if (!group || !body) return;
      group.position.set(b.x, groundY, b.z);
      // 背對最佳位置、面向煙火；倒下時往前倒
      group.rotation.y = Math.atan2(-(b.x - BEST_SPOT.x), -(b.z - BEST_SPOT.z));
      const progress = b.state === 'falling' ? b.timer / FALL_SECONDS : b.state === 'down' ? 1 : 0;
      body.rotation.x = -(Math.PI / 2) * (1 - (1 - progress) ** 2);
    });
    tooFarTimer.current = Math.max(0, tooFarTimer.current - dt);
    const eye = { x: spectator.x, y: groundY + eyeHeight, z: spectator.z };
    const clarity = viewClarity(eye, targets, blockers.current, groundY);
    const atBestSpot = Math.hypot(spectator.x - BEST_SPOT.x, spectator.z - BEST_SPOT.z) <= BEST_SPOT_RADIUS;
    if (atBestSpot && clarity === 1) stats.current.bestSeconds += dt;
    stats.current.clarity = clarity;
    stats.current.atBestSpot = atBestSpot;
    stats.current.remaining = blockers.current.filter((b) => b.state === 'standing').length;
    stats.current.tooFar = tooFarTimer.current > 0;
  });

  return <>
    {/* 最佳觀賞位置：金色光圈與光柱 */}
    <group position={[BEST_SPOT.x, groundY, BEST_SPOT.z]}>
      <mesh position={[0, 0.04, 0]} rotation={[-Math.PI / 2, 0, 0]}><ringGeometry args={[BEST_SPOT_RADIUS - 0.18, BEST_SPOT_RADIUS, 48]} /><meshBasicMaterial color="#FFC857" transparent opacity={0.9} toneMapped={false} /></mesh>
      <mesh position={[0, 3, 0]}><cylinderGeometry args={[0.05, 0.05, 6, 8]} /><meshBasicMaterial color="#FFC857" transparent opacity={0.35} toneMapped={false} /></mesh>
    </group>
    {Array.from({ length: guardCount }, (_, i) => (
      <group key={i} ref={(el) => { groups.current[i] = el; }}>
        <group ref={(el) => { bodies.current[i] = el; }}>
          <group scale={MODEL_SCALE}>
            <mesh position={[-0.2, 1, 0]}><cylinderGeometry args={[0.14, 0.14, 2, 8]} /><meshStandardMaterial color="#1f2533" /></mesh>
            <mesh position={[0.2, 1, 0]}><cylinderGeometry args={[0.14, 0.14, 2, 8]} /><meshStandardMaterial color="#1f2533" /></mesh>
            <mesh position={[0, 2.9, 0]}><cylinderGeometry args={[0.38, 0.46, 1.8, 12]} /><meshStandardMaterial color={JACKETS[i]} emissive={JACKETS[i]} emissiveIntensity={0.18} /></mesh>
            <mesh position={[-0.56, 2.8, 0]} rotation={[0, 0, 0.12]}><cylinderGeometry args={[0.1, 0.1, 1.6, 8]} /><meshStandardMaterial color={JACKETS[i]} /></mesh>
            <mesh position={[0.56, 2.8, 0]} rotation={[0, 0, -0.12]}><cylinderGeometry args={[0.1, 0.1, 1.6, 8]} /><meshStandardMaterial color={JACKETS[i]} /></mesh>
            <mesh position={[0, 4.12, 0]}><sphereGeometry args={[0.3, 16, 12]} /><meshStandardMaterial color="#e8bf9a" /></mesh>
            <mesh position={[0, 4.5, 0]}><cylinderGeometry args={[0.24, 0.3, 0.22, 12]} /><meshStandardMaterial color="#14161c" /></mesh>
          </group>
          {/* 點擊判定用的透明圓柱，大小與阻擋判定一致 */}
          <mesh ref={(el) => { colliders.current[i] = el; if (el) el.userData.blockerId = i; }} position={[0, BLOCKER_HEIGHT / 2, 0]}>
            <cylinderGeometry args={[BLOCKER_RADIUS, BLOCKER_RADIUS, BLOCKER_HEIGHT, 10]} />
            <meshBasicMaterial transparent opacity={0} depthWrite={false} />
          </mesh>
        </group>
      </group>
    ))}
  </>;
}
