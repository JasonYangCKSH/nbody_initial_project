import { Canvas, useFrame } from '@react-three/fiber';
import { OrbitControls, PerspectiveCamera } from '@react-three/drei';
import { Activity, Download, Pause, Play, RotateCcw, SkipForward } from 'lucide-react';
import { useEffect, useMemo, useRef, useState } from 'react';
import * as THREE from 'three';
import { BruteForceStructure } from './core/spatial/BruteForceStructure';
import { UniformGridStructure } from './core/spatial/UniformGridStructure';
import { Instrumentation } from './core/metrics/Instrumentation';
import { PARTICLE_RADIUS, ParticleSystem } from './core/ParticleSystem';
import { VerletBufferController } from './core/VerletBufferController';
import type { ParticleData, StepMetrics, Vec3 } from './core/types';

type Algorithm = 'Brute Force' | 'Uniform Grid';
const bounds = { x: 50, y: 50, z: 50 };
const dt = 1 / 30;

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

function SimulationView({ particles, highlighted, collisionIds }: { particles: ParticleData[]; highlighted: Set<number>; collisionIds: Set<number> }) {
  const mesh = useRef<THREE.InstancedMesh>(null);
  const dummy = useRef(new THREE.Object3D());
  useFrame(() => {
    if (!mesh.current) return;
    particles.forEach((particle, index) => {
      dummy.current.position.set(particle.position.x - bounds.x / 2, particle.position.y - bounds.y / 2, particle.position.z - bounds.z / 2);
      dummy.current.scale.setScalar(collisionIds.has(index) ? 1.7 : 1);
      dummy.current.updateMatrix();
      mesh.current!.setMatrixAt(index, dummy.current.matrix);
      mesh.current!.setColorAt(index, new THREE.Color(collisionIds.has(index) ? '#D85A30' : '#EDEAE2'));
    });
    mesh.current.instanceMatrix.needsUpdate = true;
    if (mesh.current.instanceColor) mesh.current.instanceColor.needsUpdate = true;
  });
  return <>
    <instancedMesh ref={mesh} args={[undefined, undefined, particles.length]}>
      <sphereGeometry args={[0.075, 8, 6]} />
      <meshStandardMaterial roughness={0.72} />
    </instancedMesh>
  </>;
}

function PhaseTimingChart({ history }: { history: StepMetrics[] }) {
  const width = 330;
  const height = 84;
  const points = history.slice(-40);
  if (points.length < 2) return <div className="phase-chart"><svg viewBox={`0 0 ${width} ${height}`} /></div>;
  const maxMs = Math.max(0.05, ...points.map((item) => Math.max(item.broadPhaseMs, item.narrowPhaseMs)));
  const toPath = (key: 'broadPhaseMs' | 'narrowPhaseMs') => points.map((item, index) => {
    const x = (index / (points.length - 1)) * width;
    const y = height - (item[key] / maxMs) * height;
    return `${index === 0 ? 'M' : 'L'}${x.toFixed(1)},${y.toFixed(1)}`;
  }).join(' ');
  return <div className="phase-chart">
    <svg viewBox={`0 0 ${width} ${height}`} preserveAspectRatio="none">
      <path d={toPath('broadPhaseMs')} fill="none" stroke="#5DCAA5" strokeWidth="1.5" />
      <path d={toPath('narrowPhaseMs')} fill="none" stroke="#D9A441" strokeWidth="1.5" />
    </svg>
    <div className="phase-chart-axis"><span>step {points[0].step}</span><span>step {points.at(-1)!.step}</span></div>
    <div className="phase-chart-legend"><span className="mint">● broad-phase {points.at(-1)!.broadPhaseMs.toFixed(2)}ms</span><span className="gold">● narrow-phase {points.at(-1)!.narrowPhaseMs.toFixed(2)}ms</span></div>
  </div>;
}

function App() {
  const [count, setCount] = useState(1200);
  const [algorithm, setAlgorithm] = useState<Algorithm>('Brute Force');
  const [bufferEnabled, setBufferEnabled] = useState(false);
  const [K, setK] = useState(12);
  const [cellSize, setCellSize] = useState(0.8);
  const [showGrid, setShowGrid] = useState(true);
  const [playing, setPlaying] = useState(true);
  const [version, setVersion] = useState(0);
  const [metrics, setMetrics] = useState<StepMetrics>({ step: 0, algorithm, elapsedMs: 0, broadPhaseMs: 0, narrowPhaseMs: 0, distanceChecks: 0, candidatePairs: 0, collisions: 0, rebuilt: true, rebuildCount: 1, skippedSteps: 0 });
  const [history, setHistory] = useState<StepMetrics[]>([]);
  const [events, setEvents] = useState<string[]>([]);
  const system = useRef(new ParticleSystem(count, bounds));
  const instrumentation = useRef(new Instrumentation());
  const controller = useRef(new VerletBufferController(0.15, K, dt));
  const structure = useRef(algorithm === 'Uniform Grid' ? new UniformGridStructure(bounds, cellSize) : new BruteForceStructure());
  const highlighted = useRef(new Set<number>());
  const collisionIds = useRef(new Set<number>());

  const reset = (nextAlgorithmOrEvent: Algorithm | unknown = algorithm) => { const nextAlgorithm = typeof nextAlgorithmOrEvent === 'string' ? nextAlgorithmOrEvent : algorithm; system.current = new ParticleSystem(count, bounds); structure.current = nextAlgorithm === 'Uniform Grid' ? new UniformGridStructure(bounds, cellSize) : new BruteForceStructure(); controller.current = new VerletBufferController(0.15, K, dt); setMetrics({ step: 0, algorithm: nextAlgorithm, elapsedMs: 0, broadPhaseMs: 0, narrowPhaseMs: 0, distanceChecks: 0, candidatePairs: 0, collisions: 0, rebuilt: true, rebuildCount: 1, skippedSteps: 0 }); setHistory([]); setEvents([]); setVersion((value) => value + 1); };
  const step = () => {
    const started = performance.now();
    const current = system.current;
    const particles = current.particles;
    current.step(dt);
    controller.current.K = K;
    const broadPhaseStarted = performance.now();
    const needsRebuild = !bufferEnabled || !controller.current.isListValid(particles);
    let rebuilt = false;
    if (needsRebuild) { const event = controller.current.rebuild(structure.current, particles, metrics.step + 1); rebuilt = true; if (metrics.step > 0) setEvents((old) => [`Step ${event.step}: Rebuild triggered - Particle #${event.triggeredByParticleId} exceeded skin (dx=${event.displacement.toFixed(2)} > skin=${event.skinAtTrigger.toFixed(2)})`, ...old].slice(0, 12)); }
    const pairs = structure.current.queryCandidatePairs(bufferEnabled);
    const broadPhaseMs = performance.now() - broadPhaseStarted;
    const narrowPhaseStarted = performance.now();
    highlighted.current = new Set(pairs.flat());
    const collisionPairs = pairs.filter(([a, b]) => { const p = particles[a]; const q = particles[b]; return Math.hypot(p.position.x - q.position.x, p.position.y - q.position.y, p.position.z - q.position.z) <= p.radius + q.radius; });
    current.resolveCollisions(collisionPairs);
    collisionIds.current = new Set(collisionPairs.flat());
    const narrowPhaseMs = performance.now() - narrowPhaseStarted;
    const structureMetrics = structure.current.getMetrics();
    const next: StepMetrics = { step: metrics.step + 1, algorithm, elapsedMs: performance.now() - started, broadPhaseMs, narrowPhaseMs, distanceChecks: structureMetrics.distanceChecks, candidatePairs: pairs.length, collisions: collisionPairs.length, rebuilt, rebuildCount: metrics.rebuildCount + (rebuilt ? 1 : 0), skippedSteps: metrics.skippedSteps + (rebuilt ? 0 : 1) };
    instrumentation.current.recordStep(next); setMetrics(next); setHistory(instrumentation.current.getHistory()); setVersion((value) => value + 1);
  };
  useEffect(() => { if (!playing) return; const timer = window.setInterval(step, 140); return () => window.clearInterval(timer); });
  useEffect(() => { controller.current.K = K; }, [K]);
  const latest = history.at(-2);
  const skippedRatio = metrics.step ? Math.round((metrics.skippedSteps / metrics.step) * 100) : 0;
  return <main className="app-shell">
    <section className="viewport"><Canvas dpr={[1, 1.5]}><PerspectiveCamera makeDefault position={[8, 6, 11]} fov={42} /><OrbitControls enableDamping dampingFactor={0.1} minDistance={3} maxDistance={60} makeDefault /><color attach="background" args={['#0D1420']} /><ambientLight intensity={1.5} /><pointLight position={[4, 8, 6]} intensity={30} color="#D9A441" /><SimulationView particles={system.current.particles} highlighted={highlighted.current} collisionIds={collisionIds.current} />{algorithm === 'Uniform Grid' && showGrid && <BoundingGridLines bounds={bounds} cellSize={cellSize} color="#8FD9FF" />}</Canvas><div className="viewport-label"><span className="live-dot" /> LIVE SIMULATION <b>uniform_cloud</b></div></section>
    <aside className="panel"><header><div><span className="eyebrow">COLLISION LAB / PHASE 01</span><h1>Broad-phase<br /><em>diagnostics</em></h1></div><Activity size={22} color="#5DCAA5" /></header>
      <div className="control-block"><div className="block-heading">CONTROL DECK <span>01</span></div><label>PARTICLE COUNT <strong>{count.toLocaleString()}</strong></label><input type="range" min="100" max="10000" step="100" value={count} onChange={(event) => { setCount(Number(event.target.value)); }} onMouseUp={reset} /><div className="range-endpoints"><span>100</span><span>10,000</span></div><label>SPATIAL STRUCTURE</label><div className="segmented">{(['Brute Force', 'Uniform Grid'] as Algorithm[]).map((item) => <button className={algorithm === item ? 'active' : ''} onClick={() => { setAlgorithm(item); reset(item); }} key={item}>{item}</button>)}</div>{algorithm === 'Uniform Grid' && <><label>GRID CELL SIZE <strong>{cellSize.toFixed(2)}</strong></label><input type="range" min={2 * PARTICLE_RADIUS} max="3" step="0.05" value={cellSize} onChange={(event) => setCellSize(Number(event.target.value))} onMouseUp={reset} /><div className="range-endpoints"><span>{(2 * PARTICLE_RADIUS).toFixed(2)}</span><span>3.00</span></div><div className="toggle-row"><span>SHOW GRID</span><button className={`switch ${showGrid ? 'on' : ''}`} onClick={() => setShowGrid(!showGrid)}><span /></button></div></>}<div className="toggle-row"><span>VERLET BUFFER <small>Condition 5</small></span><button className={`switch ${bufferEnabled ? 'on' : ''}`} onClick={() => { setBufferEnabled(!bufferEnabled); reset(); }}><span /></button></div><label className={bufferEnabled ? '' : 'muted'}>K SKIN COEFFICIENT <strong>{K}</strong></label><input disabled={!bufferEnabled} type="range" min="0" max="200" value={K} onChange={(event) => setK(Number(event.target.value))} /><div className="action-row"><button onClick={() => setPlaying(!playing)} title={playing ? 'Pause' : 'Play'}>{playing ? <Pause size={16} /> : <Play size={16} />}{playing ? 'PAUSE' : 'PLAY'}</button><button onClick={step} title="Step"><SkipForward size={16} /> STEP</button><button onClick={reset} title="Reset"><RotateCcw size={16} /></button></div></div>
      <div className="control-block stats"><div className="block-heading">RUN TELEMETRY <span>02</span></div><div className="stat-grid"><div><small>STEP</small><strong>{metrics.step.toString().padStart(5, '0')}</strong></div><div><small>LAST ΔT</small><strong>{metrics.elapsedMs.toFixed(2)}<i> ms</i></strong></div><div><small>DISTANCE CHECKS</small><strong>{metrics.distanceChecks.toLocaleString()}</strong><span className="compare">vs {latest?.distanceChecks.toLocaleString() ?? '---'}</span></div><div><small>CANDIDATE PAIRS</small><strong>{metrics.candidatePairs.toLocaleString()}</strong><span className="compare mint">{metrics.collisions} collisions</span></div></div><div className="mini-chart">{history.slice(-34).map((item, index) => <span key={`${item.step}-${index}`} style={{ height: `${Math.min(100, Math.max(8, item.distanceChecks / Math.max(metrics.distanceChecks, 1) * 100))}%` }} />)}</div><div className="footer-stats"><span>REBUILDS <b>{metrics.rebuildCount}</b></span><span>SKIPPED <b className="mint">{skippedRatio}%</b></span></div></div>
      <div className="control-block phase-block"><div className="block-heading">PHASE TIMING <span>03</span></div><PhaseTimingChart history={history} /></div>
      <div className="control-block event-log"><div className="block-heading">EVENT LOG <span>04</span></div>{events.length ? events.map((event, index) => <p key={`${event}-${index}`}>{event}</p>) : <p className="quiet">Awaiting rebuild trigger...</p>}</div><button className="export" onClick={() => { const blob = new Blob([instrumentation.current.exportJSON()], { type: 'application/json' }); const link = document.createElement('a'); link.href = URL.createObjectURL(blob); link.download = 'collision-lab-run.json'; link.click(); }}><Download size={14} /> EXPORT RUN DATA</button>
    </aside>
  </main>;
}
export default App;
