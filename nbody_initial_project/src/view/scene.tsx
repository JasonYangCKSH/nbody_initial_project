// =============================================================================
// 3D 畫面裡的各種物件（粒子、格線、八元樹方框、標記、煙火夜景、鏡頭控制）
// 每個元件都收 bounds（模擬空間大小）：模擬空間是 [0, bounds]，畫面上以原點為中心，所以位置都要減掉 bounds / 2
// =============================================================================
import { useFrame, useThree } from '@react-three/fiber';
import { Billboard, Stars } from '@react-three/drei';
import { useEffect, useMemo, useRef } from 'react';
import * as THREE from 'three';
import { SHELL_COLORS, type Scene } from '../core/ParticleSystem';
import { shellFractions } from '../core/scenario';
import type { ParticleData, Vec3 } from '../core/types';

export const COLLISION_COLOR = '#D85A30';
export const TRIGGER_COLOR = '#B98CFF';
export const SELECTED_COLOR = '#5DCAA5';

export const toScene = (p: Vec3, bounds: Vec3) => new THREE.Vector3(p.x - bounds.x / 2, p.y - bounds.y / 2, p.z - bounds.z / 2);

// 均勻網格的線框：只畫「裡面有粒子」的格子（和 UniformGridStructure 的 cellOf() 用同一種切法），
// 再加上模擬空間的外框。整個空間每邊約 84 格，全部畫出來會變成一整片看不清楚的密網。
// rebuildKey 變了（重建過）才重新計算，因為格子的分配只在重建時更新。
export function BoundingGridLines({ bounds, cellSize, color, particles, rebuildKey }: { bounds: Vec3; cellSize: number; color: string; particles: ParticleData[]; rebuildKey: number }) {
  const geometry = useMemo(() => {
    const points: number[] = [];
    const box = (x0: number, y0: number, z0: number, x1: number, y1: number, z1: number) => points.push(
      x0, y0, z0, x1, y0, z0, x0, y1, z0, x1, y1, z0, x0, y0, z1, x1, y0, z1, x0, y1, z1, x1, y1, z1,
      x0, y0, z0, x0, y1, z0, x1, y0, z0, x1, y1, z0, x0, y0, z1, x0, y1, z1, x1, y0, z1, x1, y1, z1,
      x0, y0, z0, x0, y0, z1, x1, y0, z0, x1, y0, z1, x0, y1, z0, x0, y1, z1, x1, y1, z0, x1, y1, z1);
    box(0, 0, 0, bounds.x, bounds.y, bounds.z);
    const occupied = new Set<string>();
    particles.forEach(({ position: p }) => {
      const cx = Math.floor(p.x / cellSize), cy = Math.floor(p.y / cellSize), cz = Math.floor(p.z / cellSize);
      const key = `${cx}:${cy}:${cz}`;
      if (occupied.has(key)) return;
      occupied.add(key);
      box(cx * cellSize, cy * cellSize, cz * cellSize, (cx + 1) * cellSize, (cy + 1) * cellSize, (cz + 1) * cellSize);
    });
    const geom = new THREE.BufferGeometry();
    geom.setAttribute('position', new THREE.Float32BufferAttribute(points, 3));
    return geom;
  }, [bounds.x, bounds.y, bounds.z, cellSize, particles, rebuildKey]); // eslint-disable-line react-hooks/exhaustive-deps
  useEffect(() => () => geometry.dispose(), [geometry]);
  return <group position={[-bounds.x / 2, -bounds.y / 2, -bounds.z / 2]}>
    <lineSegments geometry={geometry}>
      <lineBasicMaterial color={color} transparent opacity={0.55} />
    </lineSegments>
  </group>;
}

// 模擬空間的外框（小空間場景一律畫出來，觀眾才知道球是在盒子裡彈）；只畫 12 條邊，不畫面上的對角線
export function WorldBox({ bounds, color }: { bounds: Vec3; color: string }) {
  const geometry = useMemo(() => new THREE.EdgesGeometry(new THREE.BoxGeometry(bounds.x, bounds.y, bounds.z)), [bounds.x, bounds.y, bounds.z]);
  useEffect(() => () => geometry.dispose(), [geometry]);
  return <lineSegments geometry={geometry} raycast={() => null}>
    <lineBasicMaterial color={color} transparent opacity={0.6} />
  </lineSegments>;
}

// 八元樹各層的線框顏色（淺層藍 → 深層紅），避開觸發粒子的紫色與選取的薄荷綠
const DEPTH_COLORS = ['#4C7DFF', '#36B9E0', '#7BD389', '#D9E36B', '#FFC857', '#FF9F5A', '#FF6B8B', '#E0E0E0'];
export const depthColor = (depth: number) => DEPTH_COLORS[Math.min(depth, DEPTH_COLORS.length - 1)];
// 樸素配色：格線一律用同一種淡灰藍，畫面上的顏色只留給候選配對、碰撞、觸發與選取
export const PLAIN_LINE_COLOR = '#93A7B0';
export const PLAIN_SPARK_COLOR = '#F2E6CC';

export type OctreeBox = { center: Vec3; halfExtent: number; depth: number };
const WHITE = new THREE.Color('#ffffff');

// dashed：已再往下切開的中間節點，用較淡的虛線和葉節點區分；plain：樸素配色，不分層上色
export function OctreeLines({ boxes, bounds, color, dashed = false, plain = false }: { boxes: OctreeBox[]; bounds: Vec3; color?: string; dashed?: boolean; plain?: boolean }) {
  const geometry = useMemo(() => {
    const signs: [number, number, number][] = [[-1,-1,-1],[1,-1,-1],[1,1,-1],[-1,1,-1],[-1,-1,1],[1,-1,1],[1,1,1],[-1,1,1]];
    const edges: [number, number][] = [[0,1],[1,2],[2,3],[3,0],[4,5],[5,6],[6,7],[7,4],[0,4],[1,5],[2,6],[3,7]];
    const points: number[] = [];
    const colors: number[] = [];
    const distances: number[] = [];
    const rgb = new THREE.Color();
    boxes.forEach(({ center, halfExtent, depth }) => {
      const v = signs.map(([sx, sy, sz]) => [center.x + sx * halfExtent, center.y + sy * halfExtent, center.z + sz * halfExtent]);
      rgb.set(color ?? (plain ? PLAIN_LINE_COLOR : depthColor(depth)));
      if (dashed) rgb.lerp(WHITE, 0.35); // 虛線較細碎，調亮一些才看得清楚
      edges.forEach(([a, b]) => {
        points.push(...v[a], ...v[b]);
        colors.push(rgb.r, rgb.g, rgb.b, rgb.r, rgb.g, rgb.b);
        // 每條邊的虛線距離正規化成 0→1，不論格子大小，每條邊都是固定段數的虛線，小格子才不會變成實線
        distances.push(0, 1);
      });
    });
    const geom = new THREE.BufferGeometry();
    geom.setAttribute('position', new THREE.Float32BufferAttribute(points, 3));
    geom.setAttribute('color', new THREE.Float32BufferAttribute(colors, 3));
    if (dashed) geom.setAttribute('lineDistance', new THREE.Float32BufferAttribute(distances, 1));
    return geom;
  }, [boxes, color, dashed, plain]);
  return <group position={[-bounds.x / 2, -bounds.y / 2, -bounds.z / 2]}>
    <lineSegments geometry={geometry}>
      {dashed
        ? <lineDashedMaterial vertexColors transparent opacity={0.85} dashSize={0.09} gapSize={0.07} />
        : <lineBasicMaterial vertexColors transparent opacity={color ? 0.9 : 0.55} />}
    </lineSegments>
  </group>;
}

// 滑鼠鎖定（觀眾視角）時游標固定在畫面中央，射線一律從準心發出；否則用游標位置
function pointerNdc(event: PointerEvent, canvas: HTMLCanvasElement): THREE.Vector2 {
  if (document.pointerLockElement === canvas) return new THREE.Vector2(0, 0);
  const rect = canvas.getBoundingClientRect();
  return new THREE.Vector2(((event.clientX - rect.left) / rect.width) * 2 - 1, -((event.clientY - rect.top) / rect.height) * 2 + 1);
}

// 碰撞標示的時間（毫秒）：先閃白光，維持碰撞色，再慢慢淡回原本的顏色，總共約 1.2 秒，觀眾才來得及看到
const COLLISION_FLASH_MS = 180;
const COLLISION_HOLD_MS = 700;
const COLLISION_FADE_MS = 1200;

// visibleDepths 不為 null 時，只顯示葉節點在這些層的粒子（依 leafDepths）；選取與觸發粒子一律保留
// baseColors：每顆粒子的顯示顏色（煙火場景）；glow：改用不受燈光影響的發光材質
// pairColors：候選配對上色開啟時，每顆粒子的配對顏色（不在名單裡的是 null，顯示成灰色）
// collisionPairs / stepId：這一步碰撞的配對；stepId 變了代表跑了新的一步，記下碰撞時間
// dotScale：只放大「畫出來的」大小（劇本二從遠處看大量小粒子時用），物理上的半徑不變
export function SimulationView({ particles, bounds, collisionPairs, stepId, pairColors, triggerId, selectedId, onSelect, visibleDepths, leafDepths, baseColors, glow, dotScale = 1 }: { particles: ParticleData[]; bounds: Vec3; collisionPairs: [number, number][]; stepId: number; pairColors: (THREE.Color | null)[] | null; triggerId: number | null; selectedId: number | null; onSelect: (id: number) => void; visibleDepths: Set<number> | null; leafDepths: number[]; baseColors: string[] | null; glow: boolean; dotScale?: number }) {
  const mesh = useRef<THREE.InstancedMesh>(null);
  const dummy = useRef(new THREE.Object3D());
  const tint = useRef(new THREE.Color());
  const colors = useMemo(() => ({ normal: new THREE.Color('#EDEAE2'), idle: new THREE.Color('#7D878E'), flash: new THREE.Color('#FFFFFF'), collision: new THREE.Color(COLLISION_COLOR), trigger: new THREE.Color(TRIGGER_COLOR), selected: new THREE.Color(SELECTED_COLOR) }), []);
  const perParticle = useMemo(() => baseColors?.map((hex) => new THREE.Color(hex)) ?? null, [baseColors]);
  // 每顆粒子最近一次碰撞的時間（performance.now()）；換了一批粒子就重新記。
  // 這一步正在碰撞的粒子一律維持碰撞色：暫停在碰撞的那一幀時，標示不會自己淡掉
  const hitAt = useMemo(() => new Float64Array(particles.length).fill(-Infinity), [particles]);
  const collidingNow = useMemo(() => new Set(collisionPairs.flat()), [collisionPairs]);
  useEffect(() => {
    const now = performance.now();
    collisionPairs.forEach(([a, b]) => { hitAt[a] = now; hitAt[b] = now; });
  }, [stepId, hitAt]); // eslint-disable-line react-hooks/exhaustive-deps
  const camera = useThree((state) => state.camera);
  const canvas = useThree((state) => state.gl.domElement);
  const onSelectRef = useRef(onSelect);
  onSelectRef.current = onSelect;
  // 點選粒子：直接監聽原生指標事件並自行做射線檢測。
  // OrbitControls 按下時會 setPointerCapture，導致 R3F 的 onClick／onPointerUp 收不到真實滑鼠事件；
  // 按下到放開移動超過 5px 視為拖曳鏡頭，不算點選。
  useEffect(() => {
    const raycaster = new THREE.Raycaster();
    let start: { x: number; y: number } | null = null;
    const down = (event: PointerEvent) => { start = event.button === 0 ? { x: event.clientX, y: event.clientY } : null; };
    const up = (event: PointerEvent) => {
      const from = start; start = null;
      if (!from || !mesh.current || Math.hypot(event.clientX - from.x, event.clientY - from.y) > 5) return;
      raycaster.setFromCamera(pointerNdc(event, canvas), camera);
      const hit = raycaster.intersectObject(mesh.current)[0];
      if (hit?.instanceId !== undefined) onSelectRef.current(hit.instanceId);
    };
    canvas.addEventListener('pointerdown', down);
    window.addEventListener('pointerup', up);
    return () => { canvas.removeEventListener('pointerdown', down); window.removeEventListener('pointerup', up); };
  }, [camera, canvas]);
  // 粒子會移動，固定用整個模擬範圍當包圍球，點擊時的射線檢測才不會漏掉跑出初始範圍的粒子
  useEffect(() => { if (mesh.current) mesh.current.boundingSphere = new THREE.Sphere(new THREE.Vector3(), Math.hypot(bounds.x, bounds.y, bounds.z)); }, [particles, bounds]);
  // 粒子很小（半徑 0.075）時，選取、觸發、碰撞中的粒子要放大才找得到；
  // 大球（小空間場景）一律用真實大小，否則看起來會像「還沒碰到就撞了」
  const enlarge = (particles[0]?.radius ?? 0) < 0.15;
  useFrame(() => {
    if (!mesh.current) return;
    const now = performance.now();
    particles.forEach((particle, index) => {
      const isSelected = index === selectedId;
      const isTrigger = index === triggerId;
      const age = now - hitAt[index];
      const colliding = collidingNow.has(index);
      const hit = colliding || age < COLLISION_FADE_MS;
      dummy.current.position.set(particle.position.x - bounds.x / 2, particle.position.y - bounds.y / 2, particle.position.z - bounds.z / 2);
      const hidden = visibleDepths !== null && !isSelected && !isTrigger && !visibleDepths.has(leafDepths[index]);
      // 縮放成 0 即隱藏，射線也打不到，點不到被隱藏的粒子；球體幾何的半徑是 1，乘上粒子半徑就是真實大小
      const factor = hidden ? 0 : !enlarge ? 1 : isSelected || isTrigger ? 2.4 : hit ? 1.7 : glow ? 1.3 : 1;
      dummy.current.scale.setScalar(particle.radius * factor * dotScale);
      dummy.current.updateMatrix();
      mesh.current!.setMatrixAt(index, dummy.current.matrix);
      const base = pairColors ? pairColors[index] ?? colors.idle : perParticle?.[index] ?? colors.normal;
      let color = base;
      if (isSelected) color = colors.selected;
      else if (isTrigger) color = colors.trigger;
      else if (hit) {
        // 白光 → 碰撞色 → 淡回原本的顏色
        color = age < COLLISION_FLASH_MS ? tint.current.copy(colors.flash).lerp(colors.collision, age / COLLISION_FLASH_MS)
          : colliding || age < COLLISION_HOLD_MS ? colors.collision
            : tint.current.copy(colors.collision).lerp(base, (age - COLLISION_HOLD_MS) / (COLLISION_FADE_MS - COLLISION_HOLD_MS));
      }
      mesh.current!.setColorAt(index, color);
    });
    mesh.current.instanceMatrix.needsUpdate = true;
    if (mesh.current.instanceColor) mesh.current.instanceColor.needsUpdate = true;
  });
  // 粒子少時用較細緻的球，大球才不會稜角分明
  const detail = particles.length <= 500 ? [24, 16] : [8, 6];
  return <>
    <instancedMesh ref={mesh} args={[undefined, undefined, particles.length]}
      onPointerOver={() => { document.body.style.cursor = 'pointer'; }} onPointerOut={() => { document.body.style.cursor = ''; }}>
      <sphereGeometry args={[1, detail[0], detail[1]]} />
      {glow ? <meshBasicMaterial toneMapped={false} /> : <meshStandardMaterial roughness={0.72} />}
    </instancedMesh>
  </>;
}

// ---------- 煙火場景的夜景：漸層夜空、星星、草地、遠山，以及煙火炸開時照亮地面的閃光 ----------
const SKY_VERTEX = `varying vec3 vWorld; void main() { vWorld = (modelMatrix * vec4(position, 1.0)).xyz; gl_Position = projectionMatrix * viewMatrix * vec4(vWorld, 1.0); }`;
// 依高度從地平線的深藍紫漸層到頭頂的近黑色
const SKY_FRAGMENT = `varying vec3 vWorld; uniform vec3 horizon; uniform vec3 zenith;
void main() { float h = clamp(normalize(vWorld).y * 1.6 + 0.15, 0.0, 1.0); gl_FragColor = vec4(mix(horizon, zenith, pow(h, 0.7)), 1.0); }`;

export function NightScene({ step, shellCount, bounds }: { step: number; shellCount: number; bounds: Vec3 }) {
  const groundY = -bounds.y / 2;
  const skyUniforms = useMemo(() => ({ horizon: { value: new THREE.Color('#2a2350') }, zenith: { value: new THREE.Color('#03050d') } }), []);
  // 草：數千根細長的三角錐，位置與深淺固定（以固定亂數產生，每次重新整理都一樣）
  const grass = useRef<THREE.InstancedMesh>(null);
  const GRASS_COUNT = 6000;
  useEffect(() => {
    if (!grass.current) return;
    let seed = 7;
    const random = () => { seed = (seed * 1664525 + 1013904223) >>> 0; return seed / 4294967296; };
    const dummy = new THREE.Object3D();
    const color = new THREE.Color();
    for (let i = 0; i < GRASS_COUNT; i += 1) {
      const height = 0.35 + random() * 0.6;
      dummy.position.set((random() - 0.5) * 110, groundY + height / 2, (random() - 0.5) * 90 - 5);
      dummy.rotation.set((random() - 0.5) * 0.35, random() * Math.PI, (random() - 0.5) * 0.35);
      dummy.scale.set(1, height, 1);
      dummy.updateMatrix();
      grass.current.setMatrixAt(i, dummy.matrix);
      grass.current.setColorAt(i, color.setHSL(0.27 + random() * 0.08, 0.45, 0.12 + random() * 0.1));
    }
    grass.current.instanceMatrix.needsUpdate = true;
    if (grass.current.instanceColor) grass.current.instanceColor.needsUpdate = true;
  }, [groundY]);
  // 煙火閃光：每發一盞同色點光源，炸開瞬間最亮，約 1 秒內衰減
  const flash = 160 * Math.exp(-step / 25);
  return <>
    <mesh renderOrder={-1}>
      <sphereGeometry args={[300, 32, 16]} />
      <shaderMaterial side={THREE.BackSide} depthWrite={false} uniforms={skyUniforms} vertexShader={SKY_VERTEX} fragmentShader={SKY_FRAGMENT} />
    </mesh>
    <Stars radius={180} depth={60} count={2500} factor={5} saturation={0} fade speed={0.6} />
    <hemisphereLight args={['#3b4a8a', '#0b1a10', 0.35]} />
    <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, groundY - 0.01, 0]}>
      <planeGeometry args={[400, 400]} />
      <meshStandardMaterial color="#0d2414" roughness={1} />
    </mesh>
    <instancedMesh ref={grass} args={[undefined, undefined, GRASS_COUNT]} raycast={() => null}>
      <coneGeometry args={[0.05, 1, 3]} />
      <meshStandardMaterial roughness={0.9} />
    </instancedMesh>
    {/* 遠方山丘剪影 */}
    {[[-70, -90, 40, 12], [10, -110, 55, 16], [85, -95, 45, 11]].map(([x, z, w, h], i) => (
      <mesh key={i} position={[x, groundY, z]} scale={[w, h, 18]}><sphereGeometry args={[1, 24, 12, 0, Math.PI * 2, 0, Math.PI / 2]} /><meshStandardMaterial color="#070c14" roughness={1} /></mesh>
    ))}
    {shellFractions(shellCount).map((f, i) => (
      <pointLight key={i} position={[(f.x - 0.5) * bounds.x, (f.y - 0.5) * bounds.y, (f.z - 0.5) * bounds.z]} color={SHELL_COLORS[i % SHELL_COLORS.length]} intensity={flash} distance={60} decay={1.2} />
    ))}
    <fog attach="fog" args={['#120f2a', 60, 170]} />
  </>;
}

// 煙火尾跡：每顆粒子往速度反方向拖一條線，頭端是粒子顏色、尾端淡成黑色；加法混色下黑色等於透明，看起來像逐漸消失的光痕
const TRAIL_SECONDS = 0.18;
export function SparkTrails({ particles, baseColors, bounds }: { particles: ParticleData[]; baseColors: string[]; bounds: Vec3 }) {
  const geometry = useMemo(() => {
    const geom = new THREE.BufferGeometry();
    geom.setAttribute('position', new THREE.Float32BufferAttribute(new Float32Array(particles.length * 6), 3));
    const colors = new Float32Array(particles.length * 6);
    const rgb = new THREE.Color();
    baseColors.forEach((hex, index) => { rgb.set(hex); colors.set([rgb.r, rgb.g, rgb.b], index * 6); }); // 尾端保持 0（黑）
    geom.setAttribute('color', new THREE.Float32BufferAttribute(colors, 3));
    return geom;
  }, [particles, baseColors]);
  useEffect(() => () => geometry.dispose(), [geometry]);
  useFrame(() => {
    const position = geometry.getAttribute('position') as THREE.BufferAttribute;
    const array = position.array as Float32Array;
    particles.forEach((p, index) => {
      const x = p.position.x - bounds.x / 2, y = p.position.y - bounds.y / 2, z = p.position.z - bounds.z / 2;
      array.set([x, y, z, x - p.velocity.x * TRAIL_SECONDS, y - p.velocity.y * TRAIL_SECONDS, z - p.velocity.z * TRAIL_SECONDS], index * 6);
    });
    position.needsUpdate = true;
    geometry.computeBoundingSphere();
  });
  return <lineSegments geometry={geometry}>
    <lineBasicMaterial vertexColors transparent opacity={0.9} blending={THREE.AdditiveBlending} depthWrite={false} toneMapped={false} />
  </lineSegments>;
}

// 定位標記：一律面向鏡頭的圓環 + 往上延伸到模擬範圍頂端的光柱 + 半徑為 r + skin 的線框球。
// 小粒子的圓環固定 0.32 才找得到；大球的圓環緊貼在球外
export function ParticleMarker({ particle, color, bounds }: { particle: ParticleData; color: string; bounds: Vec3 }) {
  const group = useRef<THREE.Group>(null);
  const skinSphere = useRef<THREE.Mesh>(null);
  const beam = useRef<THREE.Mesh>(null);
  const ringInner = Math.max(0.32, particle.radius * 1.35);
  useFrame(() => {
    if (!group.current) return;
    group.current.position.copy(toScene(particle.position, bounds));
    skinSphere.current?.scale.setScalar(particle.radius + particle.skin);
    const height = Math.max(bounds.y - particle.position.y, 0.01);
    beam.current?.scale.set(1, height, 1);
    beam.current?.position.set(0, height / 2, 0);
  });
  return <group ref={group}>
    <Billboard>
      <mesh><ringGeometry args={[ringInner, ringInner * 1.25, 40]} /><meshBasicMaterial color={color} transparent opacity={0.9} depthTest={false} /></mesh>
    </Billboard>
    <mesh ref={skinSphere}><sphereGeometry args={[1, 16, 12]} /><meshBasicMaterial color={color} wireframe transparent opacity={0.35} /></mesh>
    <mesh ref={beam}><cylinderGeometry args={[0.012, 0.012, 1, 6]} /><meshBasicMaterial color={color} transparent opacity={0.5} /></mesh>
  </group>;
}

// 鍵盤移動視角：W/S 前後、A/D 左右、Q/E 下上，按住 Shift 加速。
// 鏡頭與 OrbitControls 的旋轉中心一起平移，滑鼠旋轉時才不會突然跳回原本的中心。
const MOVE_KEYS = new Set(['KeyW', 'KeyA', 'KeyS', 'KeyD', 'KeyQ', 'KeyE', 'ShiftLeft', 'ShiftRight']);
// enabled 為 false 時（煙火場景改由觀眾小人使用 WASD）不移動鏡頭
export function KeyboardMove({ speed = 6, enabled = true }: { speed?: number; enabled?: boolean }) {
  const camera = useThree((state) => state.camera);
  const controls = useThree((state) => state.controls) as unknown as { target: THREE.Vector3; update: () => void } | null;
  const pressed = usePressedKeys();
  const forward = useRef(new THREE.Vector3());
  const right = useRef(new THREE.Vector3());
  const move = useRef(new THREE.Vector3());
  useFrame((_, delta) => {
    const keys = pressed.current;
    if (!enabled || !keys.size) return;
    camera.getWorldDirection(forward.current);
    right.current.crossVectors(forward.current, camera.up).normalize();
    move.current.set(0, 0, 0);
    if (keys.has('KeyW')) move.current.add(forward.current);
    if (keys.has('KeyS')) move.current.sub(forward.current);
    if (keys.has('KeyD')) move.current.add(right.current);
    if (keys.has('KeyA')) move.current.sub(right.current);
    if (keys.has('KeyE')) move.current.add(camera.up);
    if (keys.has('KeyQ')) move.current.sub(camera.up);
    if (move.current.lengthSq() === 0) return;
    const fast = keys.has('ShiftLeft') || keys.has('ShiftRight') ? 3 : 1;
    move.current.normalize().multiplyScalar(speed * fast * Math.min(delta, 0.1));
    camera.position.add(move.current);
    if (controls) { controls.target.add(move.current); controls.update(); }
  });
  return null;
}

// ---------- 地上的觀眾：WASD 走路、Q/E 轉向；觀眾視角下像 Minecraft 一樣鎖定滑鼠、移動滑鼠就轉視角 ----------
// 煙火場景的模擬空間固定是邊長 25 的立方體，觀眾站在它下方的草地上
export type Spectator = { x: number; z: number; yaw: number; pitch: number };
// 站在模擬空間前方的草地上，面向煙火、稍微抬頭
export const SPECTATOR_START: Spectator = { x: 0, z: 17, yaw: 0, pitch: 0.42 };
const GROUND_Y = -25 / 2;
const EYE_HEIGHT = 1.7;

export function SpectatorModel({ spectator, visible }: { spectator: Spectator; visible: boolean }) {
  const group = useRef<THREE.Group>(null);
  useFrame(() => {
    if (!group.current) return;
    group.current.position.set(spectator.x, GROUND_Y, spectator.z);
    group.current.rotation.y = spectator.yaw;
  });
  // 模型正面朝 -z（與鏡頭預設方向一致），右手舉起指向天空
  return <group ref={group} visible={visible}>
    <mesh position={[-0.11, 0.42, 0]}><cylinderGeometry args={[0.08, 0.08, 0.84, 8]} /><meshStandardMaterial color="#2b3a55" /></mesh>
    <mesh position={[0.11, 0.42, 0]}><cylinderGeometry args={[0.08, 0.08, 0.84, 8]} /><meshStandardMaterial color="#2b3a55" /></mesh>
    <mesh position={[0, 1.17, 0]}><cylinderGeometry args={[0.2, 0.24, 0.7, 12]} /><meshStandardMaterial color="#5DCAA5" /></mesh>
    <mesh position={[-0.3, 1.12, 0]} rotation={[0, 0, -0.15]}><cylinderGeometry args={[0.06, 0.06, 0.62, 8]} /><meshStandardMaterial color="#5DCAA5" /></mesh>
    <mesh position={[0.32, 1.62, -0.12]} rotation={[0.45, 0, -0.25]}><cylinderGeometry args={[0.06, 0.06, 0.62, 8]} /><meshStandardMaterial color="#5DCAA5" /></mesh>
    <mesh position={[0, 1.68, 0]}><sphereGeometry args={[0.17, 16, 12]} /><meshStandardMaterial color="#f1c9a5" /></mesh>
    {/* 腳下的定位圈與面向箭頭，從遠處也找得到小人 */}
    <mesh position={[0, 0.03, 0]} rotation={[-Math.PI / 2, 0, 0]}><ringGeometry args={[0.55, 0.68, 40]} /><meshBasicMaterial color="#5DCAA5" transparent opacity={0.75} toneMapped={false} /></mesh>
    <mesh position={[0, 0.04, -0.95]} rotation={[-Math.PI / 2, 0, 0]}><coneGeometry args={[0.16, 0.36, 3]} /><meshBasicMaterial color="#5DCAA5" toneMapped={false} /></mesh>
  </group>;
}

// 鏡頭的垂直視角。直式畫面（手機）左右比較窄：把垂直視角加大，左右看得到的範圍和寬螢幕差不多，模擬空間才不會被切掉
export function ResponsiveFov({ fov }: { fov: number }) {
  const camera = useThree((state) => state.camera) as THREE.PerspectiveCamera;
  const aspect = useThree((state) => state.size.width / Math.max(1, state.size.height));
  useEffect(() => {
    const minAspect = 1.25;
    const halfTan = Math.tan(THREE.MathUtils.degToRad(fov) / 2);
    camera.fov = aspect >= minAspect ? fov : THREE.MathUtils.radToDeg(2 * Math.atan(halfTan * minAspect / aspect));
    camera.updateProjectionMatrix();
  }, [camera, fov, aspect]);
  return null;
}

export function SpectatorController({ spectator, active, firstPerson, onLockChange, speed = 4 }: { spectator: Spectator; active: boolean; firstPerson: boolean; onLockChange: (locked: boolean) => void; speed?: number }) {
  const camera = useThree((state) => state.camera) as THREE.PerspectiveCamera;
  const canvas = useThree((state) => state.gl.domElement);
  const controls = useThree((state) => state.controls) as unknown as { enabled: boolean } | null;
  const pressed = usePressedKeys();
  // 觀眾視角：關掉 OrbitControls；回到自由視角時恢復（視野大小由 ResponsiveFov 依視角模式調整）
  useEffect(() => {
    if (controls) controls.enabled = !firstPerson;
  }, [firstPerson, controls]);
  // Minecraft 式視角：觀眾視角下點一下畫面就鎖定滑鼠（游標隱藏），之後移動滑鼠直接轉動視角；按 Esc 放開
  const onLockChangeRef = useRef(onLockChange);
  onLockChangeRef.current = onLockChange;
  useEffect(() => {
    const locked = () => document.pointerLockElement === canvas;
    const lockChange = () => onLockChangeRef.current(locked());
    document.addEventListener('pointerlockchange', lockChange);
    if (!firstPerson) {
      if (locked()) document.exitPointerLock();
      return () => document.removeEventListener('pointerlockchange', lockChange);
    }
    const click = () => { if (!locked()) canvas.requestPointerLock()?.catch?.(() => {}); };
    const move = (event: MouseEvent) => {
      if (!locked()) return;
      spectator.yaw -= event.movementX * 0.0025;
      spectator.pitch = Math.min(1.45, Math.max(-1.45, spectator.pitch - event.movementY * 0.0025));
    };
    canvas.addEventListener('click', click);
    document.addEventListener('mousemove', move);
    return () => {
      canvas.removeEventListener('click', click);
      document.removeEventListener('mousemove', move);
      document.removeEventListener('pointerlockchange', lockChange);
    };
  }, [firstPerson, canvas, spectator]);
  useFrame((_, delta) => {
    if (!active) return;
    const keys = pressed.current;
    // 每幀最多算 0.25 秒：切回分頁時不會一下跳很遠，畫面較慢時走路速度也接近正常
    const dt = Math.min(delta, 0.25);
    const fast = keys.has('ShiftLeft') || keys.has('ShiftRight') ? 2.5 : 1;
    if (keys.has('KeyQ')) spectator.yaw += 1.6 * dt;
    if (keys.has('KeyE')) spectator.yaw -= 1.6 * dt;
    // 依小人面向的方向前後左右走（面向 -z 時 yaw = 0）
    const forwardX = -Math.sin(spectator.yaw), forwardZ = -Math.cos(spectator.yaw);
    let dx = 0, dz = 0;
    if (keys.has('KeyW')) { dx += forwardX; dz += forwardZ; }
    if (keys.has('KeyS')) { dx -= forwardX; dz -= forwardZ; }
    if (keys.has('KeyD')) { dx -= forwardZ; dz += forwardX; }
    if (keys.has('KeyA')) { dx += forwardZ; dz -= forwardX; }
    const length = Math.hypot(dx, dz);
    if (length > 0) {
      spectator.x = Math.min(50, Math.max(-50, spectator.x + (dx / length) * speed * fast * dt));
      spectator.z = Math.min(45, Math.max(-50, spectator.z + (dz / length) * speed * fast * dt));
    }
    if (firstPerson) {
      camera.position.set(spectator.x, GROUND_Y + EYE_HEIGHT, spectator.z);
      camera.rotation.set(spectator.pitch, spectator.yaw, 0, 'YXZ');
    }
  });
  return null;
}

// 追蹤目前按住的移動鍵（在輸入框打字時不算）；視窗失焦時全部放開，避免一直往同方向飄
function usePressedKeys() {
  const pressed = useRef(new Set<string>());
  useEffect(() => {
    const isTyping = (event: KeyboardEvent) => {
      const el = event.target as HTMLElement | null;
      return !!el && (el.isContentEditable || el.tagName === 'TEXTAREA' || (el.tagName === 'INPUT' && (el as HTMLInputElement).type !== 'range' && (el as HTMLInputElement).type !== 'checkbox'));
    };
    const down = (event: KeyboardEvent) => { if (MOVE_KEYS.has(event.code) && !isTyping(event) && !event.ctrlKey && !event.metaKey && !event.altKey) pressed.current.add(event.code); };
    const up = (event: KeyboardEvent) => { pressed.current.delete(event.code); };
    const clear = () => pressed.current.clear();
    window.addEventListener('keydown', down);
    window.addEventListener('keyup', up);
    window.addEventListener('blur', clear);
    return () => { window.removeEventListener('keydown', down); window.removeEventListener('keyup', up); window.removeEventListener('blur', clear); };
  }, []);
  return pressed;
}

// focus / resetView 每次遞增就執行一次：把鏡頭拉到選取的粒子旁，或回到初始視角
export type CameraPose = { position: [number, number, number]; target: [number, number, number] };
// 粒子雲：原本的斜上方視角；煙火：退到遠處、從低處往上看，像在地面看煙火
const CLOUD_POSE: CameraPose = { position: [8, 6, 11], target: [0, 0, 0] };
// 退到斜前方約 53 的距離：視角 42° 下可容納整個 25×25×25 的模擬空間，也看得到站在前方草地上的觀眾
const FIREWORK_POSE: CameraPose = { position: [12, -2, 52], target: [0, -1, 0] };
// 小空間：整個盒子都要在畫面內，沿粒子雲視角的方向退到約 2.2 倍邊長的距離。
// wide（劇本二的上下畫面）：粒子雲也退到盒子外面（約 1.25 倍邊長），看得到各個群聚，鏡頭不會夾在粒子中間
export const homePoseFor = (scene: Scene, worldSize: number, wide = false): CameraPose => {
  if (scene === 'explosion') return FIREWORK_POSE;
  if (worldSize >= 20 && !wide) return CLOUD_POSE;
  const scale = ((worldSize >= 20 ? 1.25 : 2.2) * worldSize) / Math.hypot(...CLOUD_POSE.position);
  return { position: CLOUD_POSE.position.map((v) => v * scale) as [number, number, number], target: [0, 0, 0] };
};

export function CameraRig({ focusTarget, focusToken, resetToken, homePose, bounds }: { focusTarget: ParticleData | null; focusToken: number; resetToken: number; homePose: CameraPose; bounds: Vec3 }) {
  const camera = useThree((state) => state.camera);
  const controls = useThree((state) => state.controls) as unknown as { target: THREE.Vector3; update: () => void } | null;
  useEffect(() => {
    if (!focusToken || !focusTarget || !controls) return;
    const target = toScene(focusTarget.position, bounds);
    const offset = camera.position.clone().sub(controls.target).setLength(Math.max(5, focusTarget.radius * 14));
    controls.target.copy(target);
    camera.position.copy(target).add(offset);
    controls.update();
  }, [focusToken]); // eslint-disable-line react-hooks/exhaustive-deps
  useEffect(() => {
    if (!resetToken || !controls) return;
    controls.target.set(...homePose.target);
    camera.position.set(...homePose.position);
    controls.update();
  }, [resetToken]); // eslint-disable-line react-hooks/exhaustive-deps
  return null;
}
