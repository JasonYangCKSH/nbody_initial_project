// =============================================================================
// 一個模擬畫面（一個 Canvas）：畫出一份模擬的粒子、候選配對、碰撞效果、格線與標記。
// 劇本二會同時放兩個（上均勻網格、下八元樹），兩個畫面的鏡頭用 CameraSync 同步
// =============================================================================
import { Canvas, useThree } from '@react-three/fiber';
import { OrbitControls, PerspectiveCamera } from '@react-three/drei';
import { useEffect, useMemo, type ReactNode } from 'react';
import type * as THREE from 'three';
import { OctreeStructure } from '../core/spatial/OctreeStructure';
import type { SimulationRunner } from '../core/SimulationRunner';
import { CollisionBursts, FocusHalos, PairLines, pairColorsFor } from './effects';
import { BoundingGridLines, CameraRig, homePoseFor, KeyboardMove, NightScene, OctreeLines, ParticleMarker, PLAIN_LINE_COLOR, PLAIN_SPARK_COLOR, ResponsiveFov, SELECTED_COLOR, SimulationView, SparkTrails, SpectatorController, SpectatorModel, TRIGGER_COLOR, WorldBox, type CameraPose, type Spectator } from './scene';

// 候選配對太多時不畫連線（畫面會變成一團線），只上色；粒子太多時不放碰撞光環
const MAX_PAIR_LINES = 3000;
const MAX_BURST_PARTICLES = 3000;

// 八元樹切割畫面要用的資料（側邊欄的層級清單也用同一份）：只在重建後重新計算。
// allSplits：沒有選層級時，也把每一層已切開的格子畫出來（劇本三用；粒子多時會太密，所以平常不畫）
export function useOctreeView(runner: SimulationRunner, showGrid: boolean, depthFilter: Set<number>, showSplits: boolean, onlyLevelParticles: boolean, allSplits = false) {
  const active = runner.config.algorithm === 'Octree' && showGrid;
  const rebuildCount = runner.metrics.rebuildCount;
  const anyDepthSelected = depthFilter.size > 0;
  const geometry = useMemo(() => {
    if (!active) return { boxes: [], splits: [] };
    const debug = runner.structure.getDebugGeometry();
    return debug.kind === 'octree' ? debug : { boxes: [], splits: [] };
  }, [runner, active, rebuildCount]);
  // 各層的葉節點數、已切開節點數與邊長（第 0、1 層通常只有已切開的節點）
  const depthStats = useMemo(() => {
    const byDepth = new Map<number, { depth: number; size: number; count: number; splitCount: number }>();
    const entryOf = (depth: number, halfExtent: number) => { const entry = byDepth.get(depth) ?? { depth, size: halfExtent * 2, count: 0, splitCount: 0 }; byDepth.set(depth, entry); return entry; };
    geometry.boxes.forEach(({ depth, halfExtent }) => { entryOf(depth, halfExtent).count += 1; });
    geometry.splits.forEach(({ depth, halfExtent }) => { entryOf(depth, halfExtent).splitCount += 1; });
    return [...byDepth.values()].sort((a, b) => a.depth - b.depth);
  }, [geometry]);
  const visibleBoxes = useMemo(() => (!anyDepthSelected ? geometry.boxes : geometry.boxes.filter((box) => depthFilter.has(box.depth))), [geometry, depthFilter, anyDepthSelected]);
  // 平常只在選了某一層時才畫已切開的節點，全部層級一起畫會和葉節點重疊
  const visibleSplits = useMemo(() => (!anyDepthSelected ? (allSplits ? geometry.splits : []) : !showSplits ? [] : geometry.splits.filter((box) => depthFilter.has(box.depth))), [geometry, depthFilter, showSplits, anyDepthSelected, allSplits]);
  // 每顆粒子所在葉節點的層級（最近一次重建時的結果）
  const leafDepths = useMemo(() => (active && runner.structure instanceof OctreeStructure ? runner.structure.getLeafDepths() : []), [runner, active, rebuildCount]);
  const particleCountAtDepth = leafDepths.filter((depth) => depthFilter.has(depth)).length;
  const particleDepthFilter = active && anyDepthSelected && onlyLevelParticles ? depthFilter : null;
  return { geometry, depthStats, visibleBoxes, visibleSplits, leafDepths, particleCountAtDepth, particleDepthFilter };
}

// 劇本二的上下兩個畫面共用同一個視角：轉動、縮放或移動其中一個，另一個跟著動
export type CameraSyncHub = { views: Set<{ camera: THREE.Camera; controls: { target: THREE.Vector3; update: () => void } }>; syncing: boolean };
export const createCameraSyncHub = (): CameraSyncHub => ({ views: new Set(), syncing: false });

function CameraSync({ hub }: { hub: CameraSyncHub }) {
  const camera = useThree((state) => state.camera);
  const controls = useThree((state) => state.controls) as unknown as (THREE.EventDispatcher<{ change: object }> & { target: THREE.Vector3; update: () => void }) | null;
  useEffect(() => {
    if (!controls) return;
    const self = { camera, controls };
    // 後來才出現的畫面（例如剛切到劇本二的下方畫面）先對齊已經在的畫面
    const existing = [...hub.views][0];
    if (existing) {
      camera.position.copy(existing.camera.position);
      controls.target.copy(existing.controls.target);
      controls.update();
    }
    hub.views.add(self);
    const onChange = () => {
      // 正在把別的畫面的視角複製過來時，不要再複製回去（否則會互相觸發）
      if (hub.syncing) return;
      hub.syncing = true;
      hub.views.forEach((other) => {
        if (other === self) return;
        other.camera.position.copy(camera.position);
        other.controls.target.copy(controls.target);
        other.controls.update();
      });
      hub.syncing = false;
    };
    controls.addEventListener('change', onChange);
    return () => { controls.removeEventListener('change', onChange); hub.views.delete(self); };
  }, [camera, controls, hub]);
  return null;
}

export interface ScreenOptions {
  pairColorsOn: boolean;
  plain: boolean;
  showGrid: boolean;
  depthFilter: Set<number>;
  showSplits: boolean;
  allSplits: boolean;
  onlyLevelParticles: boolean;
  autoRotate: boolean;
  selectedId: number | null;
  onSelect: (id: number) => void;
  focusToken: number;
  resetViewToken: number;
  wide: boolean;          // 劇本二的上下畫面：鏡頭退到盒子外面，小粒子畫大一點
  viewMode: 'orbit' | 'person';
  spectator: Spectator;
  onLockChange: (locked: boolean) => void;
}

// primary：只有第一個畫面接收鍵盤移動；兩個畫面都移動的話，同步後會變成移動兩倍
export function SimulationScreen({ runner, options, cameraSync, primary = true, children }: { runner: SimulationRunner; options: ScreenOptions; cameraSync?: CameraSyncHub; primary?: boolean; children?: ReactNode }) {
  const { pairColorsOn, plain, showGrid, depthFilter, showSplits, allSplits, onlyLevelParticles, autoRotate, selectedId, onSelect, focusToken, resetViewToken, wide, viewMode, spectator, onLockChange } = options;
  const { metrics, events, bounds } = runner;
  const { algorithm, scene, bufferEnabled } = runner.config;
  // 鏡頭的起始位置。數值沒變時 R3F 不會重設鏡頭，所以使用者轉過的視角不會每一步被拉回來
  const homePose: CameraPose = homePoseFor(scene, bounds.x, wide);
  const dotScale = wide && runner.config.radius < 0.1 ? 2 : 1;
  const octree = useOctreeView(runner, showGrid, depthFilter, showSplits, onlyLevelParticles, allSplits);
  const triggerId = bufferEnabled && events.length ? events[0].triggeredByParticleId : null;
  const selected = selectedId !== null ? runner.particles[selectedId] ?? null : null;
  // 葉節點只在重建時改變，所以依 rebuildCount 重新查詢
  const selectedLeaf = useMemo(() => (selected && runner.structure instanceof OctreeStructure ? runner.structure.getLeafOf(selected.id) : null),
    [runner, selectedId, metrics.rebuildCount]); // eslint-disable-line react-hooks/exhaustive-deps
  // 候選配對的顏色只在名單換過（重建）時重新分配；附近 8 個半徑內的群組盡量不同色
  const pairColors = useMemo(() => (pairColorsOn ? pairColorsFor(runner.pairs, runner.particles, Math.max(1, 8 * runner.config.radius), runner.listVersion * 3) : null),
    [runner, runner.listVersion, pairColorsOn]); // eslint-disable-line react-hooks/exhaustive-deps
  // 煙火顏色：樸素配色時全部用同一種暖白色
  const sparkColors = useMemo(() => (runner.system.colors && plain ? runner.system.colors.map(() => PLAIN_SPARK_COLOR) : runner.system.colors), [runner, plain]);
  const firstPerson = scene === 'explosion' && viewMode === 'person';
  return <div className="sim-screen">
    <Canvas dpr={[1, 1.5]}>
      <PerspectiveCamera makeDefault position={homePose.position} fov={42} />
      {/* 上下雙畫面又寬又矮：垂直視角放寬到 55°，整個模擬空間才放得進去 */}
      <ResponsiveFov fov={firstPerson ? 62 : wide ? 55 : 42} />
      <OrbitControls enableDamping dampingFactor={0.1} minDistance={Math.min(3, bounds.x * 0.12)} maxDistance={bounds.x * 2.4} autoRotate={autoRotate && !firstPerson} autoRotateSpeed={0.8} makeDefault />
      {cameraSync && <CameraSync hub={cameraSync} />}
      {scene !== 'explosion' && <color attach="background" args={['#0D1420']} />}
      {scene === 'explosion' && <NightScene step={metrics.step} shellCount={runner.config.clusters} bounds={bounds} />}
      <ambientLight intensity={1.5} />
      <pointLight position={[4, 8, 6]} intensity={30} color="#D9A441" />
      <SimulationView particles={runner.particles} bounds={bounds} collisionPairs={runner.collisionPairs} stepId={metrics.step} pairColors={pairColors} triggerId={triggerId} selectedId={selectedId} onSelect={onSelect} visibleDepths={octree.particleDepthFilter} leafDepths={octree.leafDepths} baseColors={sparkColors} glow={scene === 'explosion'} dotScale={dotScale} />
      {pairColors && runner.pairs.length <= MAX_PAIR_LINES && <PairLines pairs={runner.pairs} particles={runner.particles} bounds={bounds} pairColors={pairColors} />}
      {runner.particles.length <= MAX_BURST_PARTICLES && <CollisionBursts pairs={runner.newCollisionPairs} stepId={metrics.step} particles={runner.particles} bounds={bounds} />}
      <FocusHalos ids={runner.system.focusIds} particles={runner.particles} bounds={bounds} />
      {scene === 'micro' && <WorldBox bounds={bounds} color="#5F7F8C" />}
      {algorithm === 'Uniform Grid' && showGrid && <BoundingGridLines bounds={bounds} cellSize={runner.cellSize} color={plain ? PLAIN_LINE_COLOR : '#8FD9FF'} particles={runner.particles} rebuildKey={metrics.rebuildCount} />}
      {algorithm === 'Octree' && showGrid && <OctreeLines boxes={octree.visibleBoxes} bounds={bounds} plain={plain} />}
      {algorithm === 'Octree' && showGrid && octree.visibleSplits.length > 0 && <OctreeLines boxes={octree.visibleSplits} bounds={bounds} dashed plain={plain} />}
      {triggerId !== null && triggerId !== selectedId && runner.particles[triggerId] && <ParticleMarker particle={runner.particles[triggerId]} color={TRIGGER_COLOR} bounds={bounds} />}
      {selected && <ParticleMarker particle={selected} color={SELECTED_COLOR} bounds={bounds} />}
      {selectedLeaf && <OctreeLines boxes={[selectedLeaf]} bounds={bounds} color={SELECTED_COLOR} />}
      <CameraRig focusTarget={selected} focusToken={focusToken} resetToken={resetViewToken} homePose={homePose} bounds={bounds} />
      {scene === 'explosion' && sparkColors && <SparkTrails particles={runner.particles} baseColors={sparkColors} bounds={bounds} />}
      <KeyboardMove enabled={primary && scene !== 'explosion'} speed={bounds.x * 0.24} />
      {scene === 'explosion' && <SpectatorModel spectator={spectator} visible={viewMode === 'orbit'} />}
      <SpectatorController spectator={spectator} active={scene === 'explosion'} firstPerson={firstPerson} onLockChange={onLockChange} />
    </Canvas>
    {children}
  </div>;
}
