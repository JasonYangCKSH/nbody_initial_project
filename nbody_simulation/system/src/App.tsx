import { Canvas, useFrame } from '@react-three/fiber';
import { OrbitControls, PerspectiveCamera } from '@react-three/drei';
import { Activity, Download, Pause, Play, RotateCcw, SkipForward } from 'lucide-react';
import { useEffect, useMemo, useRef, useState } from 'react';
import * as THREE from 'three';
import { BruteForceStructure } from './core/spatial/BruteForceStructure';
import { OctreeStructure } from './core/spatial/OctreeStructure';
import { UniformGridStructure } from './core/spatial/UniformGridStructure';
import { Instrumentation } from './core/metrics/Instrumentation';
import { DEFAULT_ACCELERATION, ParticleSystem } from './core/ParticleSystem';
import { VerletBufferController } from './core/VerletBufferController';
import type { ParticleData, StepMetrics, Vec3 } from './core/types';

type Algorithm = 'Brute Force' | 'Uniform Grid' | 'Octree';
const bounds = { x:25, y: 25, z: 25 };
const dt = 1 / 60;
// 與 C++ bench 一致：cellSize = cellSizeRatio(2) × 2 × PARTICLE_RADIUS(0.075)
const CELL_SIZE = 0.3;

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

function PhaseChart({ history, metricKey, color, label, markRebuilds }: { history: StepMetrics[]; metricKey: 'broadPhaseMs' | 'narrowPhaseMs'; color: string; label: string; markRebuilds?: boolean }) {
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
    <div className="phase-chart-axis"><span>step {points[0].step}</span><span>step {points.at(-1)!.step}</span></div>
    <div className="phase-chart-legend"><span style={{ color }}>● {label} {latest[metricKey].toFixed(2)}ms</span>{markRebuilds && <span className={latest.rebuilt ? 'rebuild-flag on' : 'rebuild-flag'}>● rebuilt this step: {latest.rebuilt ? 'yes' : 'no'}</span>}</div>
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
  const [playing, setPlaying] = useState(true);
  const [version, setVersion] = useState(0);
  const [metrics, setMetrics] = useState<StepMetrics>(() => initialMetrics(algorithm));
  // step() 以 ref 為準累加，避免同一次 render 內多次呼叫（interval + STEP 按鈕）讀到舊的 metrics 而少算
  const metricsRef = useRef(metrics);
  const [history, setHistory] = useState<StepMetrics[]>([]);
  const [events, setEvents] = useState<string[]>([]);
  const system = useRef(new ParticleSystem(count, bounds, clusterFactor, acceleration));
  const instrumentation = useRef(new Instrumentation());
  const controller = useRef(new VerletBufferController(0.15, K, dt));
  const structure = useRef(algorithm === 'Uniform Grid' ? new UniformGridStructure(bounds, CELL_SIZE) : algorithm === 'Octree' ? new OctreeStructure(bounds, 20, 100) : new BruteForceStructure());
  const highlighted = useRef(new Set<number>());
  const collisionIds = useRef(new Set<number>());
  const cachedPairs = useRef<[number, number][]>([]);

  const reset = (nextAlgorithmOrEvent: Algorithm | unknown = algorithm) => { const nextAlgorithm = typeof nextAlgorithmOrEvent === 'string' ? nextAlgorithmOrEvent : algorithm; system.current = new ParticleSystem(count, bounds, clusterFactor, acceleration); structure.current = nextAlgorithm === 'Uniform Grid' ? new UniformGridStructure(bounds, CELL_SIZE) : nextAlgorithm === 'Octree' ? new OctreeStructure(bounds, 6, 8) : new BruteForceStructure(); controller.current = new VerletBufferController(0.15, K, dt); cachedPairs.current = []; instrumentation.current = new Instrumentation(); metricsRef.current = initialMetrics(nextAlgorithm); setMetrics(metricsRef.current); setHistory([]); setEvents([]); setVersion((value) => value + 1); };
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
      if (prev.step > 0)
        setEvents((old) => [`Step ${event.step}: Rebuild triggered - Particle #${event.triggeredByParticleId} exceeded skin (dx=${event.displacement.toFixed(2)} > skin=${event.skinAtTrigger.toFixed(2)})`, ...old].slice(0, 12));
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
          <SimulationView particles={system.current.particles} highlighted={highlighted.current} collisionIds={collisionIds.current} />
          {algorithm === 'Uniform Grid' && showGrid && <BoundingGridLines bounds={bounds} cellSize={CELL_SIZE} color="#8FD9FF" />}
        </Canvas>
        <div className="viewport-label">
          <span className="live-dot" /> LIVE SIMULATION <b>{clusterFactor > 0 ? `spatial_cluster (${clusterFactor.toFixed(2)})` : 'uniform_cloud'}</b>
        </div>
      </section>

      <aside className="panel">
        <header>
          <div>
            <span className="eyebrow">COLLISION LAB</span>
            <h1>
              Broad-phase<br />
              <em>diagnostics</em>
            </h1>
          </div>
          <Activity size={22} color="#5DCAA5" />
        </header>

        <div className="control-block">
          <div className="block-heading">CONTROL DECK <span>01</span></div>

          <label>
            PARTICLE COUNT <strong>{count.toLocaleString()}</strong>
          </label>
          <input type="range" min="1000" max="50000" step="100" value={count} onChange={(event) => { setCount(Number(event.target.value)); }} onMouseUp={reset} />
          <div className="range-endpoints"><span>100</span><span>10,000</span></div>

          <label>
            CLUSTER FACTOR <strong>{clusterFactor.toFixed(2)}</strong>
          </label>
          <input type="range" min="0" max="1" step="0.01" value={clusterFactor} onChange={(event) => { setClusterFactor(Number(event.target.value)); }} onMouseUp={reset} />
          <div className="range-endpoints"><span>0.00</span><span>1.00</span></div>

          <label>
            ACCELERATION <strong>±{acceleration.toFixed(2)}</strong>
          </label>
          <input type="range" min="0" max="2" step="0.05" value={acceleration} onChange={(event) => { setAcceleration(Number(event.target.value)); }} onMouseUp={reset} />
          <div className="range-endpoints"><span>0.00</span><span>2.00</span></div>

          <label>SPATIAL STRUCTURE</label>
          <div className="segmented">
            {(['Brute Force', 'Uniform Grid', 'Octree'] as Algorithm[]).map((item) => (
              <button className={algorithm === item ? 'active' : ''} onClick={() => { setAlgorithm(item); reset(item); }} key={item}>
                {item}
              </button>
            ))}
          </div>

          {algorithm === 'Uniform Grid' && (
            <>
              <div className="toggle-row">
                <span>SHOW GRID</span>
                <button className={`switch ${showGrid ? 'on' : ''}`} onClick={() => setShowGrid(!showGrid)}>
                  <span />
                </button>
              </div>
            </>
          )}

          <div className="toggle-row">
            <span>VERLET BUFFER <small>Condition 5</small></span>
            <button className={`switch ${bufferEnabled ? 'on' : ''}`} onClick={() => { setBufferEnabled(!bufferEnabled); reset(); }}>
              <span />
            </button>
          </div>

          <label className={bufferEnabled ? '' : 'muted'}>
            K SKIN COEFFICIENT <strong>{K}</strong>
          </label>
          <input disabled={!bufferEnabled} type="range" min="0" max="200" value={K} onChange={(event) => setK(Number(event.target.value))} />

          <div className="action-row">
            <button onClick={() => setPlaying(!playing)} title={playing ? 'Pause' : 'Play'}>
              {playing ? <Pause size={16} /> : <Play size={16} />}
              {playing ? 'PAUSE' : 'PLAY'}
            </button>
            <button onClick={step} title="Step">
              <SkipForward size={16} /> STEP
            </button>
            <button onClick={reset} title="Reset">
              <RotateCcw size={16} />
            </button>
          </div>
        </div>

        <div className="control-block stats">
          <div className="block-heading">RUN TELEMETRY <span>02</span></div>
          <div className="stat-grid">
            <div><small>STEP</small><strong>{metrics.step.toString().padStart(5, '0')}</strong></div>
            <div><small>LAST ΔT</small><strong>{metrics.elapsedMs.toFixed(2)}<i> ms</i></strong></div>
            <div>
              <small>DISTANCE CHECKS</small>
              <strong>{metrics.distanceChecks.toLocaleString()}</strong>
              <span className="compare">vs {latest?.distanceChecks.toLocaleString() ?? '---'}</span>
            </div>
            <div>
              <small>CANDIDATE PAIRS</small>
              <strong>{metrics.candidatePairs.toLocaleString()}</strong>
              <span className="compare mint">{metrics.collisions} collisions</span>
            </div>
          </div>
          <div className="mini-chart">
            {history.slice(-34).map((item, index) => (
              <span key={`${item.step}-${index}`} style={{ height: `${Math.min(100, Math.max(8, item.distanceChecks / Math.max(metrics.distanceChecks, 1) * 100))}%` }} />
            ))}
          </div>
          <div className="footer-stats">
            <span>REBUILDS <b>{metrics.rebuildCount}</b></span>
            <span>SKIPPED <b className="mint">{skippedRatio}%</b></span>
          </div>
        </div>

        <div className="control-block phase-block">
          <div className="block-heading">PHASE TIMING <span>03</span></div>
          <div className="phase-chart-title mint">BROAD-PHASE</div>
          <PhaseChart history={history} metricKey="broadPhaseMs" color="#5DCAA5" label="broad-phase" markRebuilds={bufferEnabled} />
          <div className="phase-chart-title gold">NARROW-PHASE</div>
          <PhaseChart history={history} metricKey="narrowPhaseMs" color="#D9A441" label="narrow-phase" />
        </div>

        <div className="control-block event-log">
          <div className="block-heading">EVENT LOG <span>04</span></div>
          {events.length ? events.map((event, index) => <p key={`${event}-${index}`}>{event}</p>) : <p className="quiet">Awaiting rebuild trigger...</p>}
        </div>

        <button className="export" onClick={() => { const blob = new Blob([instrumentation.current.exportJSON()], { type: 'application/json' }); const link = document.createElement('a'); link.href = URL.createObjectURL(blob); link.download = 'collision-lab-run.json'; link.click(); }}>
          <Download size={14} /> EXPORT RUN DATA
        </button>
      </aside>
    </main>
  );
}

export default App;
