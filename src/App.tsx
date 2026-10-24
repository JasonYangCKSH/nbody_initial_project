import { Canvas, useFrame, useThree } from '@react-three/fiber';
import { Billboard, OrbitControls, PerspectiveCamera } from '@react-three/drei';
import { Activity, Crosshair, Download, Languages, LocateFixed, Pause, Play, RotateCcw, SkipForward, X } from 'lucide-react';
import { useEffect, useMemo, useRef, useState } from 'react';
import * as THREE from 'three';
import { BruteForceStructure } from './core/spatial/BruteForceStructure';
import { OctreeStructure } from './core/spatial/OctreeStructure';
import { UniformGridStructure } from './core/spatial/UniformGridStructure';
import { Instrumentation } from './core/metrics/Instrumentation';
import { DEFAULT_ACCELERATION, ParticleSystem } from './core/ParticleSystem';
import { VerletBufferController } from './core/VerletBufferController';
import type { ParticleData, RebuildEvent, StepMetrics, Vec3 } from './core/types';
import { loadLang, saveLang, strings, type Algorithm, type Lang, type Strings } from './i18n';
const bounds = { x:25, y: 25, z: 25 };
const dt = 1 / 60;
// 與 C++ bench 一致：cellSize = cellSizeRatio(2) × 2 × PARTICLE_RADIUS(0.075)
const CELL_SIZE = 0.3;
// 八元樹參數：最大深度與葉節點容量（粒子數超過容量就切成 8 格，到最大深度後不再切）
const OCTREE_MAX_DEPTH = 6;
const OCTREE_LEAF_CAPACITY = 8;

// Wireframe lattice matching UniformGridStructure's cellOf() partition
// (cell boundaries at multiples of cellSize, clipped to the bounding box),
// not just a decorative floor plane.
function BoundingGridLines({ bounds, cellSize, color }: { bounds: Vec3; cellSize: number; color: string }) {
  const geometry = useMemo(() => {
    const axisSteps = (extent: number) => { const count = Math.max(1, Math.ceil(extent / cellSize)); return Array.from({ length: count + 1 }, (_, i) => Math.min(i * cellSize, extent)); };
    const xs = axisSteps(bounds.x);
    const ys = axisSteps(bounds.y);
    const zs = axisSteps(bounds.z);
    const points: number[] = [];
    ys.forEach((y) => zs.forEach((z) => points.push(0, y, z, bounds.x, y, z)));
    xs.forEach((x) => zs.forEach((z) => points.push(x, 0, z, x, bounds.y, z)));
    xs.forEach((x) => ys.forEach((y) => points.push(x, y, 0, x, y, bounds.z)));
    const geom = new THREE.BufferGeometry();
    geom.setAttribute('position', new THREE.Float32BufferAttribute(points, 3));
    return geom;
  }, [bounds.x, bounds.y, bounds.z, cellSize]);
  return <group position={[-bounds.x / 2, -bounds.y / 2, -bounds.z / 2]}>
    <lineSegments geometry={geometry}>
      <lineBasicMaterial color={color} transparent opacity={0.35} />
    </lineSegments>
  </group>;
}

// 八元樹各層的線框顏色（淺層藍 → 深層紅），避開觸發粒子的紫色與選取的薄荷綠
const DEPTH_COLORS = ['#4C7DFF', '#36B9E0', '#7BD389', '#D9E36B', '#FFC857', '#FF9F5A', '#FF6B8B', '#E0E0E0'];
const depthColor = (depth: number) => DEPTH_COLORS[Math.min(depth, DEPTH_COLORS.length - 1)];

type OctreeBox = { center: Vec3; halfExtent: number; depth: number };
const WHITE = new THREE.Color('#ffffff');

// dashed：已再往下切開的中間節點，用較淡的虛線和葉節點區分
function OctreeLines({ boxes, bounds, color, dashed = false }: { boxes: OctreeBox[]; bounds: Vec3; color?: string; dashed?: boolean }) {
  const geometry = useMemo(() => {
    const signs: [number, number, number][] = [[-1,-1,-1],[1,-1,-1],[1,1,-1],[-1,1,-1],[-1,-1,1],[1,-1,1],[1,1,1],[-1,1,1]];
    const edges: [number, number][] = [[0,1],[1,2],[2,3],[3,0],[4,5],[5,6],[6,7],[7,4],[0,4],[1,5],[2,6],[3,7]];
    const points: number[] = [];
    const colors: number[] = [];
    const distances: number[] = [];
    const rgb = new THREE.Color();
    boxes.forEach(({ center, halfExtent, depth }) => {
      const v = signs.map(([sx, sy, sz]) => [center.x + sx * halfExtent, center.y + sy * halfExtent, center.z + sz * halfExtent]);
      rgb.set(color ?? depthColor(depth));
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
  }, [boxes, color, dashed]);
  return <group position={[-bounds.x / 2, -bounds.y / 2, -bounds.z / 2]}>
    <lineSegments geometry={geometry}>
      {dashed
        ? <lineDashedMaterial vertexColors transparent opacity={0.85} dashSize={0.09} gapSize={0.07} />
        : <lineBasicMaterial vertexColors transparent opacity={color ? 0.9 : 0.55} />}
    </lineSegments>
  </group>;
}

const COLLISION_COLOR = '#D85A30';
const TRIGGER_COLOR = '#B98CFF';
const SELECTED_COLOR = '#5DCAA5';
const toScene = (p: Vec3) => new THREE.Vector3(p.x - bounds.x / 2, p.y - bounds.y / 2, p.z - bounds.z / 2);

// visibleDepths 不為 null 時，只顯示葉節點在這些層的粒子（依 leafDepths）；選取與觸發粒子一律保留
function SimulationView({ particles, highlighted, collisionIds, triggerId, selectedId, onSelect, visibleDepths, leafDepths }: { particles: ParticleData[]; highlighted: Set<number>; collisionIds: Set<number>; triggerId: number | null; selectedId: number | null; onSelect: (id: number) => void; visibleDepths: Set<number> | null; leafDepths: number[] }) {
  const mesh = useRef<THREE.InstancedMesh>(null);
  const dummy = useRef(new THREE.Object3D());
  const colors = useMemo(() => ({ normal: new THREE.Color('#EDEAE2'), collision: new THREE.Color(COLLISION_COLOR), trigger: new THREE.Color(TRIGGER_COLOR), selected: new THREE.Color(SELECTED_COLOR) }), []);
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
      const rect = canvas.getBoundingClientRect();
      raycaster.setFromCamera(new THREE.Vector2(((event.clientX - rect.left) / rect.width) * 2 - 1, -((event.clientY - rect.top) / rect.height) * 2 + 1), camera);
      const hit = raycaster.intersectObject(mesh.current)[0];
      if (hit?.instanceId !== undefined) onSelectRef.current(hit.instanceId);
    };
    canvas.addEventListener('pointerdown', down);
    window.addEventListener('pointerup', up);
    return () => { canvas.removeEventListener('pointerdown', down); window.removeEventListener('pointerup', up); };
  }, [camera, canvas]);
  // 粒子會移動，固定用整個模擬範圍當包圍球，點擊時的射線檢測才不會漏掉跑出初始範圍的粒子
  useEffect(() => { if (mesh.current) mesh.current.boundingSphere = new THREE.Sphere(new THREE.Vector3(), Math.hypot(bounds.x, bounds.y, bounds.z)); }, [particles]);
  useFrame(() => {
    if (!mesh.current) return;
    particles.forEach((particle, index) => {
      const isSelected = index === selectedId;
      const isTrigger = index === triggerId;
      dummy.current.position.set(particle.position.x - bounds.x / 2, particle.position.y - bounds.y / 2, particle.position.z - bounds.z / 2);
      const hidden = visibleDepths !== null && !isSelected && !isTrigger && !visibleDepths.has(leafDepths[index]);
      // 縮放成 0 即隱藏，射線也打不到，點不到被隱藏的粒子
      dummy.current.scale.setScalar(hidden ? 0 : isSelected || isTrigger ? 2.4 : collisionIds.has(index) ? 1.7 : 1);
      dummy.current.updateMatrix();
      mesh.current!.setMatrixAt(index, dummy.current.matrix);
      mesh.current!.setColorAt(index, isSelected ? colors.selected : isTrigger ? colors.trigger : collisionIds.has(index) ? colors.collision : colors.normal);
    });
    mesh.current.instanceMatrix.needsUpdate = true;
    if (mesh.current.instanceColor) mesh.current.instanceColor.needsUpdate = true;
  });
  return <>
    <instancedMesh ref={mesh} args={[undefined, undefined, particles.length]}
      onPointerOver={() => { document.body.style.cursor = 'pointer'; }} onPointerOut={() => { document.body.style.cursor = ''; }}>
      <sphereGeometry args={[0.075, 8, 6]} />
      <meshStandardMaterial roughness={0.72} />
    </instancedMesh>
  </>;
}

// 定位標記：一律面向鏡頭的圓環 + 往上延伸到模擬範圍頂端的光柱 + 半徑為 r + skin 的線框球
function ParticleMarker({ particle, color }: { particle: ParticleData; color: string }) {
  const group = useRef<THREE.Group>(null);
  const skinSphere = useRef<THREE.Mesh>(null);
  const beam = useRef<THREE.Mesh>(null);
  useFrame(() => {
    if (!group.current) return;
    group.current.position.copy(toScene(particle.position));
    skinSphere.current?.scale.setScalar(particle.radius + particle.skin);
    const height = Math.max(bounds.y - particle.position.y, 0.01);
    beam.current?.scale.set(1, height, 1);
    beam.current?.position.set(0, height / 2, 0);
  });
  return <group ref={group}>
    <Billboard>
      <mesh><ringGeometry args={[0.32, 0.4, 40]} /><meshBasicMaterial color={color} transparent opacity={0.9} depthTest={false} /></mesh>
    </Billboard>
    <mesh ref={skinSphere}><sphereGeometry args={[1, 16, 12]} /><meshBasicMaterial color={color} wireframe transparent opacity={0.35} /></mesh>
    <mesh ref={beam}><cylinderGeometry args={[0.012, 0.012, 1, 6]} /><meshBasicMaterial color={color} transparent opacity={0.5} /></mesh>
  </group>;
}

// 鍵盤移動視角：W/S 前後、A/D 左右、Q/E 下上，按住 Shift 加速。
// 鏡頭與 OrbitControls 的旋轉中心一起平移，滑鼠旋轉時才不會突然跳回原本的中心。
const MOVE_KEYS = new Set(['KeyW', 'KeyA', 'KeyS', 'KeyD', 'KeyQ', 'KeyE', 'ShiftLeft', 'ShiftRight']);
function KeyboardMove({ speed = 6 }: { speed?: number }) {
  const camera = useThree((state) => state.camera);
  const controls = useThree((state) => state.controls) as unknown as { target: THREE.Vector3; update: () => void } | null;
  const pressed = useRef(new Set<string>());
  useEffect(() => {
    const isTyping = (event: KeyboardEvent) => {
      const el = event.target as HTMLElement | null;
      return !!el && (el.isContentEditable || el.tagName === 'TEXTAREA' || (el.tagName === 'INPUT' && (el as HTMLInputElement).type !== 'range' && (el as HTMLInputElement).type !== 'checkbox'));
    };
    const down = (event: KeyboardEvent) => { if (MOVE_KEYS.has(event.code) && !isTyping(event) && !event.ctrlKey && !event.metaKey && !event.altKey) pressed.current.add(event.code); };
    const up = (event: KeyboardEvent) => { pressed.current.delete(event.code); };
    const clear = () => pressed.current.clear(); // 視窗失焦時放開所有按鍵，避免一直往同方向飄
    window.addEventListener('keydown', down);
    window.addEventListener('keyup', up);
    window.addEventListener('blur', clear);
    return () => { window.removeEventListener('keydown', down); window.removeEventListener('keyup', up); window.removeEventListener('blur', clear); };
  }, []);
  const forward = useRef(new THREE.Vector3());
  const right = useRef(new THREE.Vector3());
  const move = useRef(new THREE.Vector3());
  useFrame((_, delta) => {
    const keys = pressed.current;
    if (!keys.size) return;
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

// focus / resetView 每次遞增就執行一次：把鏡頭拉到選取的粒子旁，或回到初始視角
function CameraRig({ focusTarget, focusToken, resetToken }: { focusTarget: ParticleData | null; focusToken: number; resetToken: number }) {
  const camera = useThree((state) => state.camera);
  const controls = useThree((state) => state.controls) as unknown as { target: THREE.Vector3; update: () => void } | null;
  useEffect(() => {
    if (!focusToken || !focusTarget || !controls) return;
    const target = toScene(focusTarget.position);
    const offset = camera.position.clone().sub(controls.target).setLength(5);
    controls.target.copy(target);
    camera.position.copy(target).add(offset);
    controls.update();
  }, [focusToken]); // eslint-disable-line react-hooks/exhaustive-deps
  useEffect(() => {
    if (!resetToken || !controls) return;
    controls.target.set(0, 0, 0);
    camera.position.set(8, 6, 11);
    controls.update();
  }, [resetToken]); // eslint-disable-line react-hooks/exhaustive-deps
  return null;
}

function PhaseChart({ history, metricKey, color, label, markRebuilds, t }: { history: StepMetrics[]; metricKey: 'broadPhaseMs' | 'narrowPhaseMs'; color: string; label: string; markRebuilds?: boolean; t: Strings }) {
  const width = 330;
  const height = 84;
  const points = history.slice(-40);
  if (points.length < 2) return <div className="phase-chart"><svg viewBox={`0 0 ${width} ${height}`} /></div>;
  const maxMs = Math.max(0.05, ...points.map((item) => item[metricKey]));
  const xAt = (index: number) => (index / (points.length - 1)) * width;
  const yAt = (item: StepMetrics) => height - (item[metricKey] / maxMs) * height;
  const path = points.map((item, index) => `${index === 0 ? 'M' : 'L'}${xAt(index).toFixed(1)},${yAt(item).toFixed(1)}`).join(' ');
  const latest = points.at(-1)!;
  return <div className="phase-chart">
    <svg viewBox={`0 0 ${width} ${height}`} preserveAspectRatio="none">
      <path d={path} fill="none" stroke={color} strokeWidth="1.5" />
    </svg>
    <div className="phase-chart-axis"><span>{t.axisStep(points[0].step)}</span><span>{t.axisStep(points.at(-1)!.step)}</span></div>
    <div className="phase-chart-legend"><span style={{ color }}>● {label} {latest[metricKey].toFixed(2)}ms</span>{markRebuilds && <span className={latest.rebuilt ? 'rebuild-flag on' : 'rebuild-flag'}>● {t.rebuiltThisStep(latest.rebuilt)}</span>}</div>
  </div>;
}

// 第 0 幀必定重建（見 C++ Simulation::needsRebuild），計數從 0 開始，由第一次 step 計入
const initialMetrics = (algorithm: string): StepMetrics => ({ step: 0, algorithm, elapsedMs: 0, broadPhaseMs: 0, narrowPhaseMs: 0, distanceChecks: 0, candidatePairs: 0, collisions: 0, rebuilt: false, rebuildCount: 0, skippedSteps: 0 });

function App() {
  const [count, setCount] = useState(1200);
  const [clusterFactor, setClusterFactor] = useState(0);
  const [acceleration, setAcceleration] = useState(DEFAULT_ACCELERATION);
  const [algorithm, setAlgorithm] = useState<Algorithm>('Brute Force');
  const [bufferEnabled, setBufferEnabled] = useState(false);
  const [K, setK] = useState(12);
  const [showGrid, setShowGrid] = useState(false);
  // 八元樹切割只顯示選取的層級（可多選）；空集合表示顯示全部層級
  const [depthFilter, setDepthFilter] = useState<Set<number>>(() => new Set());
  const toggleDepth = (depth: number) => setDepthFilter((old) => { const next = new Set(old); if (next.has(depth)) next.delete(depth); else next.add(depth); return next; });
  const anyDepthSelected = depthFilter.size > 0;
  const [showSplits, setShowSplits] = useState(true);
  const [onlyLevelParticles, setOnlyLevelParticles] = useState(false);
  const [playing, setPlaying] = useState(true);
  const [version, setVersion] = useState(0);
  const [metrics, setMetrics] = useState<StepMetrics>(() => initialMetrics(algorithm));
  // step() 以 ref 為準累加，避免同一次 render 內多次呼叫（interval + STEP 按鈕）讀到舊的 metrics 而少算
  const metricsRef = useRef(metrics);
  const [history, setHistory] = useState<StepMetrics[]>([]);
  // 存結構化事件，切換語言時整份記錄會用新語言重新顯示
  const [events, setEvents] = useState<RebuildEvent[]>([]);
  const [selectedId, setSelectedId] = useState<number | null>(null);
  const [focusToken, setFocusToken] = useState(0);
  const [resetViewToken, setResetViewToken] = useState(0);
  // 每顆粒子觸發重建的累計次數（事件記錄只保留最近 12 筆，這裡保留全部）
  const triggerCounts = useRef(new Map<number, number>());
  const [lang, setLang] = useState<Lang>(loadLang);
  const t = strings[lang];
  useEffect(() => { document.documentElement.lang = lang === 'zh' ? 'zh-Hant' : 'en'; saveLang(lang); }, [lang]);
  const system = useRef(new ParticleSystem(count, bounds, clusterFactor, acceleration));
  const instrumentation = useRef(new Instrumentation());
  const controller = useRef(new VerletBufferController(0.15, K, dt));
  const structure = useRef(algorithm === 'Uniform Grid' ? new UniformGridStructure(bounds, CELL_SIZE) : algorithm === 'Octree' ? new OctreeStructure(bounds, OCTREE_MAX_DEPTH, OCTREE_LEAF_CAPACITY) : new BruteForceStructure());
  const highlighted = useRef(new Set<number>());
  const collisionIds = useRef(new Set<number>());
  const cachedPairs = useRef<[number, number][]>([]);

  const reset = (nextAlgorithmOrEvent: Algorithm | unknown = algorithm) => { const nextAlgorithm = typeof nextAlgorithmOrEvent === 'string' ? nextAlgorithmOrEvent : algorithm; system.current = new ParticleSystem(count, bounds, clusterFactor, acceleration); structure.current = nextAlgorithm === 'Uniform Grid' ? new UniformGridStructure(bounds, CELL_SIZE) : nextAlgorithm === 'Octree' ? new OctreeStructure(bounds, OCTREE_MAX_DEPTH, OCTREE_LEAF_CAPACITY) : new BruteForceStructure(); controller.current = new VerletBufferController(0.15, K, dt); cachedPairs.current = []; instrumentation.current = new Instrumentation(); metricsRef.current = initialMetrics(nextAlgorithm); setMetrics(metricsRef.current); setHistory([]); setEvents([]); setSelectedId(null); setDepthFilter(new Set()); triggerCounts.current = new Map(); setVersion((value) => value + 1); };
  const step = () => {
    const started = performance.now();
    const prev = metricsRef.current;
    const current = system.current;
    const particles = current.particles;
    controller.current.K = K;
    // 與 C++ Simulation::needsRebuild 相同：第 0 幀必定重建
    const needsRebuild = prev.step === 0 || !bufferEnabled || !controller.current.isListValid(particles);
    let rebuilt = false;
    let broadPhaseMs = 0;
    if (needsRebuild) {
      // broad-phase
      const broadPhaseStarted = performance.now();
      const event = controller.current.rebuild(structure.current, particles, prev.step + 1);
      rebuilt = true;
      cachedPairs.current = structure.current.queryCandidatePairs(bufferEnabled);
      broadPhaseMs = performance.now() - broadPhaseStarted;
      // end broad-phase
      // 只記錄 Verlet buffer 開啟、且確實有粒子超出自己 skin 而觸發的重建
      if (prev.step > 0 && bufferEnabled && event.displacement > event.skinAtTrigger) {
        triggerCounts.current.set(event.triggeredByParticleId, (triggerCounts.current.get(event.triggeredByParticleId) ?? 0) + 1);
        setEvents((old) => [event, ...old].slice(0, 12));
      }
    }
    
    // narrow-phase
    const pairs = cachedPairs.current;
    const narrowPhaseStarted = performance.now();
    highlighted.current = new Set(pairs.flat());
    // 與 C++ narrow::colliding 相同：(r_a + r_b)² ≥ |pos_a − pos_b|²
    const collisionPairs = pairs.filter(([a, b]) => { const p = particles[a]; const q = particles[b]; const r = p.radius + q.radius; return r * r - ((p.position.x - q.position.x) ** 2 + (p.position.y - q.position.y) ** 2 + (p.position.z - q.position.z) ** 2) >= 0; });
    collisionIds.current = new Set(collisionPairs.flat());
    const narrowPhaseMs = performance.now() - narrowPhaseStarted;
    // end narrow-phase

    // 與 C++ Simulation::step 相同：排序碰撞配對 → 碰撞回應 → 積分（含牆面反彈）
    collisionPairs.sort(([a1, b1], [a2, b2]) => a1 - a2 || b1 - b2);
    current.resolveCollisions(collisionPairs);
    current.step(dt);

    const structureMetrics = structure.current.getMetrics();
    const next: StepMetrics = { step: prev.step + 1, algorithm, elapsedMs: performance.now() - started, broadPhaseMs, narrowPhaseMs, distanceChecks: structureMetrics.distanceChecks, candidatePairs: pairs.length, collisions: collisionPairs.length, rebuilt, rebuildCount: prev.rebuildCount + (rebuilt ? 1 : 0), skippedSteps: prev.skippedSteps + (rebuilt ? 0 : 1) };
    metricsRef.current = next;
    instrumentation.current.recordStep(next); 
    setMetrics(next); 
    setHistory(instrumentation.current.getHistory()); 
    setVersion((value) => value + 1);
  };
  useEffect(() => { if (!playing) return; const timer = window.setInterval(step, 140); return () => window.clearInterval(timer); });
  useEffect(() => { controller.current.K = K; }, [K]);
  const octreeGeometry = useMemo(() => {
    if (algorithm !== 'Octree' || !showGrid) return { boxes: [], splits: [] };
    const geometry = structure.current.getDebugGeometry();
    return geometry.kind === 'octree' ? geometry : { boxes: [], splits: [] };
  }, [algorithm, showGrid, metrics.rebuildCount]);
  const octreeBoxes = octreeGeometry.boxes;
  // 各層的葉節點數、已切開節點數與邊長，供「依層級顯示」清單使用（第 0、1 層通常只有已切開的節點）
  const depthStats = useMemo(() => {
    const byDepth = new Map<number, { depth: number; size: number; count: number; splitCount: number }>();
    const entryOf = (depth: number, halfExtent: number) => { const entry = byDepth.get(depth) ?? { depth, size: halfExtent * 2, count: 0, splitCount: 0 }; byDepth.set(depth, entry); return entry; };
    octreeGeometry.boxes.forEach(({ depth, halfExtent }) => { entryOf(depth, halfExtent).count += 1; });
    octreeGeometry.splits.forEach(({ depth, halfExtent }) => { entryOf(depth, halfExtent).splitCount += 1; });
    return [...byDepth.values()].sort((a, b) => a.depth - b.depth);
  }, [octreeGeometry]);
  const visibleOctreeBoxes = useMemo(() => (!anyDepthSelected ? octreeBoxes : octreeBoxes.filter((box) => depthFilter.has(box.depth))), [octreeBoxes, depthFilter]);
  // 每顆粒子所在葉節點的層級（最近一次重建時的結果），只在八元樹切割畫面開啟時計算
  const leafDepths = useMemo(() => (algorithm === 'Octree' && showGrid && structure.current instanceof OctreeStructure ? structure.current.getLeafDepths() : []),
    [algorithm, showGrid, metrics.rebuildCount]); // eslint-disable-line react-hooks/exhaustive-deps
  const particleCountAtDepth = leafDepths.filter((depth) => depthFilter.has(depth)).length;
  const particleDepthFilter = algorithm === 'Octree' && showGrid && anyDepthSelected && onlyLevelParticles ? depthFilter : null;
  // 只在選了某一層時才畫已切開的節點，全部層級一起畫會和葉節點完全重疊
  const visibleSplitBoxes = useMemo(() => (!anyDepthSelected || !showSplits ? [] : octreeGeometry.splits.filter((box) => depthFilter.has(box.depth))), [octreeGeometry, depthFilter, showSplits]);
  const triggerId = bufferEnabled && events.length ? events[0].triggeredByParticleId : null;
  const selected = selectedId !== null ? system.current.particles[selectedId] ?? null : null;
  const selectParticle = (id: number, focus = false) => { setSelectedId(id); if (focus) setFocusToken((value) => value + 1); };
  // 葉節點只在重建時改變，所以依 rebuildCount 重新查詢
  const selectedLeaf = useMemo(() => (selected && structure.current instanceof OctreeStructure ? structure.current.getLeafOf(selected.id) : null),
    [selectedId, metrics.rebuildCount, algorithm]); // eslint-disable-line react-hooks/exhaustive-deps
  const skinCap = !selected ? null : algorithm === 'Octree' ? (selectedLeaf ? Math.max(selectedLeaf.halfExtent - selected.radius, 0) : null) : algorithm === 'Uniform Grid' ? Math.max(CELL_SIZE / 2 - selected.radius, 0) : null;
  const latest = history.at(-2);
  const skippedRatio = metrics.step ? Math.round((metrics.skippedSteps / metrics.step) * 100) : 0;

  return (
    <main className="app-shell">
      <section className="viewport">
        <Canvas dpr={[1, 1.5]}>
          <PerspectiveCamera makeDefault position={[8, 6, 11]} fov={42} />
          <OrbitControls enableDamping dampingFactor={0.1} minDistance={3} maxDistance={60} makeDefault />
          <color attach="background" args={['#0D1420']} />
          <ambientLight intensity={1.5} />
          <pointLight position={[4, 8, 6]} intensity={30} color="#D9A441" />
          <SimulationView particles={system.current.particles} highlighted={highlighted.current} collisionIds={collisionIds.current} triggerId={triggerId} selectedId={selectedId} onSelect={(id) => selectParticle(id)} visibleDepths={particleDepthFilter} leafDepths={leafDepths} />
          {algorithm === 'Uniform Grid' && showGrid && <BoundingGridLines bounds={bounds} cellSize={CELL_SIZE} color="#8FD9FF" />}
          {algorithm === 'Octree' && showGrid && <OctreeLines boxes={visibleOctreeBoxes} bounds={bounds} />}
          {algorithm === 'Octree' && showGrid && visibleSplitBoxes.length > 0 && <OctreeLines boxes={visibleSplitBoxes} bounds={bounds} dashed />}
          {triggerId !== null && triggerId !== selectedId && system.current.particles[triggerId] && <ParticleMarker particle={system.current.particles[triggerId]} color={TRIGGER_COLOR} />}
          {selected && <ParticleMarker particle={selected} color={SELECTED_COLOR} />}
          {selectedLeaf && <OctreeLines boxes={[selectedLeaf]} bounds={bounds} color={SELECTED_COLOR} />}
          <CameraRig focusTarget={selected} focusToken={focusToken} resetToken={resetViewToken} />
          <KeyboardMove />
        </Canvas>
        <div className="viewport-label">
          <span className="live-dot" /> {t.liveSimulation}
          <span className="viewport-field">{t.labelStructure} <b>{t.algorithms[algorithm]}</b></span>
          <span className="viewport-field">{t.labelDistribution} <b>{clusterFactor > 0 ? t.spatialCluster(clusterFactor.toFixed(2)) : t.uniformCloud}</b></span>
        </div>
        <div className="viewport-legend">
          <span><i style={{ background: COLLISION_COLOR }} />{t.legendCollision}</span>
          <span><i style={{ background: TRIGGER_COLOR }} />{t.legendTrigger}</span>
          <span><i style={{ background: SELECTED_COLOR }} />{t.legendSelected}</span>
          <span className="hint">{t.clickToSelect}</span>
          <span className="hint keys">{t.keyboardHint}</span>
        </div>
      </section>

      <aside className="panel">
        <header>
          <div>
            <span className="eyebrow">{t.eyebrow}</span>
            <h1>
              {t.titleMain}<br />
              <em>{t.titleSub}</em>
            </h1>
          </div>
          <div className="header-tools">
            <button className="lang-toggle" onClick={() => setLang(lang === 'en' ? 'zh' : 'en')} title={t.langToggleTitle} aria-label={t.langToggleTitle}>
              <Languages size={13} /> {t.langToggle}
            </button>
            <Activity size={22} color="#5DCAA5" />
          </div>
        </header>

        <div className="control-block">
          <div className="block-heading">{t.controlDeck} <span>01</span></div>

          <label>
            {t.particleCount} <strong>{count.toLocaleString()}</strong>
          </label>
          <input type="range" min="1000" max="50000" step="100" value={count} onChange={(event) => { setCount(Number(event.target.value)); }} onMouseUp={reset} />
          <div className="range-endpoints"><span>1,000</span><span>50,000</span></div>

          <label>
            {t.clusterFactor} <strong>{clusterFactor.toFixed(2)}</strong>
          </label>
          <input type="range" min="0" max="1" step="0.01" value={clusterFactor} onChange={(event) => { setClusterFactor(Number(event.target.value)); }} onMouseUp={reset} />
          <div className="range-endpoints"><span>0.00</span><span>1.00</span></div>

          <label>
            {t.acceleration} <strong>±{acceleration.toFixed(2)}</strong>
          </label>
          <input type="range" min="0" max="2" step="0.05" value={acceleration} onChange={(event) => { setAcceleration(Number(event.target.value)); }} onMouseUp={reset} />
          <div className="range-endpoints"><span>0.00</span><span>2.00</span></div>

          <label>{t.spatialStructure}</label>
          <div className="segmented">
            {(['Brute Force', 'Uniform Grid', 'Octree'] as Algorithm[]).map((item) => (
              <button className={algorithm === item ? 'active' : ''} onClick={() => { setAlgorithm(item); reset(item); }} key={item}>
                {t.algorithms[item]}
              </button>
            ))}
          </div>

          {(algorithm === 'Uniform Grid' || algorithm === 'Octree') && (
            <>
              <div className="toggle-row">
                <span>{algorithm === 'Octree' ? t.showOctree : t.showGrid}</span>
                <button className={`switch ${showGrid ? 'on' : ''}`} onClick={() => setShowGrid(!showGrid)}>
                  <span />
                </button>
              </div>
              {algorithm === 'Octree' && showGrid && (
                <div className="depth-list">
                  <div className="depth-list-head"><span>{t.depthLevel}</span><span>{t.depthSize}</span><span>{t.depthLeaves}</span><span>{t.depthSplits}</span></div>
                  <button className={!anyDepthSelected ? 'active' : ''} onClick={() => setDepthFilter(new Set())}>
                    <span><i className="all" />{t.depthAll}</span><span /><span>{octreeBoxes.length}</span><span>{octreeGeometry.splits.length}</span>
                  </button>
                  {depthStats.map(({ depth, size, count, splitCount }) => (
                    <button key={depth} className={depthFilter.has(depth) ? 'active' : ''} onClick={() => toggleDepth(depth)}>
                      <span><i style={{ background: depthColor(depth) }} />{t.depthName(depth)}</span><span>{size.toFixed(3)}</span><span>{count}</span><span className="split-count">{splitCount}</span>
                    </button>
                  ))}
                  <label className={`split-toggle ${!anyDepthSelected ? 'muted' : ''}`}>
                    <input type="checkbox" checked={showSplits} disabled={!anyDepthSelected} onChange={(event) => setShowSplits(event.target.checked)} />
                    <span><i className="dash" />{t.showSplits}</span>
                  </label>
                  <label className={`split-toggle ${!anyDepthSelected ? 'muted' : ''}`}>
                    <input type="checkbox" checked={onlyLevelParticles} disabled={!anyDepthSelected} onChange={(event) => setOnlyLevelParticles(event.target.checked)} />
                    <span><i className="dot" />{!anyDepthSelected ? t.onlyLevelParticles : t.onlyLevelParticlesCount(particleCountAtDepth)}</span>
                  </label>
                  <p className="depth-note">{!anyDepthSelected ? t.depthNote : t.depthNoteSelected}</p>
                </div>
              )}
            </>
          )}

          <div className="toggle-row">
            <span>{t.verletBuffer} <small>{t.condition5}</small></span>
            <button className={`switch ${bufferEnabled ? 'on' : ''}`} onClick={() => { setBufferEnabled(!bufferEnabled); reset(); }}>
              <span />
            </button>
          </div>

          <label className={bufferEnabled ? '' : 'muted'}>
            {t.kSkin} <strong>{K}</strong>
          </label>
          <input disabled={!bufferEnabled} type="range" min="0" max="200" value={K} onChange={(event) => setK(Number(event.target.value))} />

          <div className="action-row">
            <button onClick={() => setPlaying(!playing)} title={playing ? t.pause : t.play}>
              {playing ? <Pause size={16} /> : <Play size={16} />}
              {playing ? t.pause : t.play}
            </button>
            <button onClick={step} title={t.step}>
              <SkipForward size={16} /> {t.step}
            </button>
            <button onClick={reset} title={t.reset}>
              <RotateCcw size={16} />
            </button>
          </div>
        </div>

        <div className="control-block stats">
          <div className="block-heading">{t.runTelemetry} <span>02</span></div>
          <div className="stat-grid">
            <div><small>{t.stepCount}</small><strong>{metrics.step.toString().padStart(5, '0')}</strong></div>
            <div><small>{t.lastDt}</small><strong>{metrics.elapsedMs.toFixed(2)}<i> ms</i></strong></div>
            <div>
              <small>{t.distanceChecks}</small>
              <strong>{metrics.distanceChecks.toLocaleString()}</strong>
              <span className="compare">{t.vs} {latest?.distanceChecks.toLocaleString() ?? '---'}</span>
            </div>
            <div>
              <small>{t.candidatePairs}</small>
              <strong>{metrics.candidatePairs.toLocaleString()}</strong>
              <span className="compare mint">{t.collisions(metrics.collisions)}</span>
            </div>
          </div>
          <div className="mini-chart">
            {history.slice(-34).map((item, index) => (
              <span key={`${item.step}-${index}`} style={{ height: `${Math.min(100, Math.max(8, item.distanceChecks / Math.max(metrics.distanceChecks, 1) * 100))}%` }} />
            ))}
          </div>
          <div className="footer-stats">
            <span>{t.rebuilds} <b>{metrics.rebuildCount}</b></span>
            <span>{t.skipped} <b className="mint">{skippedRatio}%</b></span>
          </div>
        </div>

        <div className="control-block phase-block">
          <div className="block-heading">{t.phaseTiming} <span>03</span></div>
          <div className="phase-chart-title mint">{t.broadPhaseTitle}</div>
          <PhaseChart history={history} metricKey="broadPhaseMs" color="#5DCAA5" label={t.broadPhase} markRebuilds={bufferEnabled} t={t} />
          <div className="phase-chart-title gold">{t.narrowPhaseTitle}</div>
          <PhaseChart history={history} metricKey="narrowPhaseMs" color="#D9A441" label={t.narrowPhase} t={t} />
        </div>

        <div className="control-block event-log">
          <div className="block-heading">{t.eventLog} <span>04</span></div>
          {events.length ? events.map((event, index) => (
            <button className={`event-item ${event.triggeredByParticleId === selectedId ? 'selected' : ''}`} key={`${event.step}-${index}`} title={t.eventClickHint}
              onClick={() => selectParticle(event.triggeredByParticleId, true)}>
              {t.rebuildEvent(event)}
            </button>
          )) : <p className="quiet">{bufferEnabled ? t.awaitingRebuild : t.bufferOffNoEvents}</p>}
        </div>

        <div className="control-block inspector">
          <div className="block-heading">{t.inspector} <span>05</span></div>
          <div className="inspector-actions">
            <button disabled={triggerId === null} onClick={() => triggerId !== null && selectParticle(triggerId, true)}><LocateFixed size={13} /> {t.selectLastTrigger}</button>
            <button disabled={!selected} onClick={() => setFocusToken((value) => value + 1)}><Crosshair size={13} /> {t.focus}</button>
            <button onClick={() => setResetViewToken((value) => value + 1)}><RotateCcw size={13} /> {t.resetView}</button>
            <button disabled={!selected} onClick={() => setSelectedId(null)} title={t.clearSelection} aria-label={t.clearSelection}><X size={13} /></button>
          </div>
          {triggerId === null && <p className="trigger-hint">{bufferEnabled ? t.noTriggerYet : t.noTriggerBufferOff}</p>}
          {selected ? (() => {
            const dx = Math.hypot(selected.position.x - selected.positionAtLastBroadPhase.x, selected.position.y - selected.positionAtLastBroadPhase.y, selected.position.z - selected.positionAtLastBroadPhase.z);
            const speed = Math.hypot(selected.velocity.x, selected.velocity.y, selected.velocity.z);
            const acc = Math.hypot(selected.acceleration.x, selected.acceleration.y, selected.acceleration.z);
            const vec = (v: Vec3) => `(${v.x.toFixed(2)}, ${v.y.toFixed(2)}, ${v.z.toFixed(2)})`;
            const rows: [string, string, string?][] = [
              [t.fieldId, `#${selected.id}`],
              [t.fieldStatus, collisionIds.current.has(selected.id) ? t.statusColliding : highlighted.current.has(selected.id) ? t.statusCandidate : t.statusFree, collisionIds.current.has(selected.id) ? 'hot' : undefined],
              [t.fieldPosition, vec(selected.position)],
              [t.fieldVelocity, `${speed.toFixed(3)}  ${vec(selected.velocity)}`],
              [t.fieldAcceleration, `${acc.toFixed(3)}  ${vec(selected.acceleration)}`],
              [t.fieldRadius, selected.radius.toFixed(3)],
              [t.fieldSkin, bufferEnabled ? selected.skin.toFixed(4) : t.notApplicable],
              [t.fieldSkinCap, skinCap === null ? t.notApplicable : `${skinCap.toFixed(4)}${bufferEnabled && skinCap > 0 && selected.skin >= skinCap - 1e-9 ? `  ${t.capped}` : ''}`],
              [t.fieldDisplacement, bufferEnabled ? `${dx.toFixed(4)}  (${selected.skin > 0 ? `${((dx / selected.skin) * 100).toFixed(0)}%` : '—'} ${t.ofSkin})` : t.notApplicable, bufferEnabled && dx > selected.skin ? 'hot' : undefined],
              [t.fieldLeaf, selectedLeaf ? t.leafInfo(selectedLeaf.depth, selectedLeaf.halfExtent) : algorithm === 'Uniform Grid' ? t.gridCell(CELL_SIZE) : t.notApplicable],
              [t.fieldCapacity, selectedLeaf ? (selectedLeaf.depth >= OCTREE_MAX_DEPTH ? t.capacityUnlimited(selectedLeaf.count, OCTREE_MAX_DEPTH) : t.capacityOf(selectedLeaf.count, OCTREE_LEAF_CAPACITY)) : algorithm === 'Uniform Grid' ? t.capacityGrid : t.notApplicable,
                selectedLeaf && selectedLeaf.count > OCTREE_LEAF_CAPACITY ? 'hot' : undefined],
              [t.fieldTriggerCount, String(triggerCounts.current.get(selected.id) ?? 0)],
            ];
            return <dl className="inspector-grid">{rows.map(([label, value, tone]) => <div key={label}><dt>{label}</dt><dd className={tone ?? ''}>{value}</dd></div>)}</dl>;
          })() : <p className="quiet">{t.inspectorEmpty}</p>}
        </div>

        <button className="export" onClick={() => { const blob = new Blob([instrumentation.current.exportJSON()], { type: 'application/json' }); const link = document.createElement('a'); link.href = URL.createObjectURL(blob); link.download = 'collision-lab-run.json'; link.click(); }}>
          <Download size={14} /> {t.exportRunData}
        </button>
      </aside>
    </main>
  );
}

export default App;
