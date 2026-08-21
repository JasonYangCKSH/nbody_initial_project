import { Canvas, useFrame } from '@react-three/fiber';
import { Grid, PerspectiveCamera } from '@react-three/drei';
import { Activity, Download, Pause, Play, RotateCcw, SkipForward } from 'lucide-react';
import { useEffect, useRef, useState } from 'react';
import * as THREE from 'three';
import { BruteForceStructure } from './core/spatial/BruteForceStructure';
import { UniformGridStructure } from './core/spatial/UniformGridStructure';
import { Instrumentation } from './core/metrics/Instrumentation';
import { ParticleSystem } from './core/ParticleSystem';
import { VerletBufferController } from './core/VerletBufferController';
import type { ParticleData, StepMetrics } from './core/types';

type Algorithm = 'Brute Force' | 'Uniform Grid';
const bounds = { x: 100, y: 100, z: 100 };
const dt = 1 / 30;

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

function App() {
  const [count, setCount] = useState(1200);
  const [algorithm, setAlgorithm] = useState<Algorithm>('Uniform Grid');
  const [bufferEnabled, setBufferEnabled] = useState(true);
  const [K, setK] = useState(12);
  const [playing, setPlaying] = useState(true);
  const [version, setVersion] = useState(0);
  const [metrics, setMetrics] = useState<StepMetrics>({ step: 0, algorithm, elapsedMs: 0, distanceChecks: 0, candidatePairs: 0, collisions: 0, rebuilt: true, rebuildCount: 1, skippedSteps: 0 });
  const [history, setHistory] = useState<StepMetrics[]>([]);
  const [events, setEvents] = useState<string[]>([]);
  const system = useRef(new ParticleSystem(count, bounds));
  const instrumentation = useRef(new Instrumentation());
  const controller = useRef(new VerletBufferController(0.15, K, dt));
  const structure = useRef(algorithm === 'Uniform Grid' ? new UniformGridStructure(bounds, 0.8) : new BruteForceStructure());
  const highlighted = useRef(new Set<number>());
  const collisionIds = useRef(new Set<number>());

  const reset = (nextAlgorithmOrEvent: Algorithm | unknown = algorithm) => { const nextAlgorithm = typeof nextAlgorithmOrEvent === 'string' ? nextAlgorithmOrEvent : algorithm; system.current = new ParticleSystem(count, bounds); structure.current = nextAlgorithm === 'Uniform Grid' ? new UniformGridStructure(bounds, 0.8) : new BruteForceStructure(); controller.current = new VerletBufferController(0.15, K, dt); setMetrics({ step: 0, algorithm: nextAlgorithm, elapsedMs: 0, distanceChecks: 0, candidatePairs: 0, collisions: 0, rebuilt: true, rebuildCount: 1, skippedSteps: 0 }); setHistory([]); setEvents([]); setVersion((value) => value + 1); };
  const step = () => {
    const started = performance.now();
    const current = system.current;
    const particles = current.particles;
    current.step(dt);
    controller.current.K = K;
    const needsRebuild = !bufferEnabled || !controller.current.isListValid(particles);
    let rebuilt = false;
    if (needsRebuild) { const event = controller.current.rebuild(structure.current, particles, metrics.step + 1); rebuilt = true; if (metrics.step > 0) setEvents((old) => [`Step ${event.step}: Rebuild triggered - Particle #${event.triggeredByParticleId} exceeded skin (dx=${event.displacement.toFixed(2)} > skin=${event.skinAtTrigger.toFixed(2)})`, ...old].slice(0, 12)); }
    const pairs = structure.current.queryCandidatePairs(bufferEnabled);
    highlighted.current = new Set(pairs.flat());
    collisionIds.current = new Set(pairs.filter(([a, b]) => { const p = particles[a]; const q = particles[b]; return Math.hypot(p.position.x - q.position.x, p.position.y - q.position.y, p.position.z - q.position.z) <= p.radius + q.radius; }).flat());
    const structureMetrics = structure.current.getMetrics();
    const next: StepMetrics = { step: metrics.step + 1, algorithm, elapsedMs: performance.now() - started, distanceChecks: structureMetrics.distanceChecks, candidatePairs: pairs.length, collisions: collisionIds.current.size / 2, rebuilt, rebuildCount: metrics.rebuildCount + (rebuilt ? 1 : 0), skippedSteps: metrics.skippedSteps + (rebuilt ? 0 : 1) };
    instrumentation.current.recordStep(next); setMetrics(next); setHistory(instrumentation.current.getHistory()); setVersion((value) => value + 1);
  };
  useEffect(() => { if (!playing) return; const timer = window.setInterval(step, 140); return () => window.clearInterval(timer); });
  useEffect(() => { controller.current.K = K; }, [K]);
  const latest = history.at(-2);
  const skippedRatio = metrics.step ? Math.round((metrics.skippedSteps / metrics.step) * 100) : 0;
  return <main className="app-shell">
    <section className="viewport"><Canvas dpr={[1, 1.5]}><PerspectiveCamera makeDefault position={[8, 6, 11]} fov={42} /><color attach="background" args={['#0D1420']} /><ambientLight intensity={1.5} /><pointLight position={[4, 8, 6]} intensity={30} color="#D9A441" /><SimulationView particles={system.current.particles} highlighted={highlighted.current} collisionIds={collisionIds.current} /><Grid args={[16, 16]} position={[0, -bounds.y / 2, 0]} cellSize={0.8} sectionColor="#2A3F4A" cellColor="#172934" sectionThickness={0.7} cellThickness={0.4} /></Canvas><div className="viewport-label"><span className="live-dot" /> LIVE SIMULATION <b>uniform_cloud</b></div></section>
    <aside className="panel"><header><div><span className="eyebrow">COLLISION LAB / PHASE 01</span><h1>Broad-phase<br /><em>diagnostics</em></h1></div><Activity size={22} color="#5DCAA5" /></header>
      <div className="control-block"><div className="block-heading">CONTROL DECK <span>01</span></div><label>PARTICLE COUNT <strong>{count.toLocaleString()}</strong></label><input type="range" min="1000" max="10000" step="100" value={count} onChange={(event) => { setCount(Number(event.target.value)); }} onMouseUp={reset} /><div className="range-endpoints"><span>1,000</span><span>5,000</span></div><label>SPATIAL STRUCTURE</label><div className="segmented">{(['Brute Force', 'Uniform Grid'] as Algorithm[]).map((item) => <button className={algorithm === item ? 'active' : ''} onClick={() => { setAlgorithm(item); reset(); }} key={item}>{item}</button>)}</div><div className="toggle-row"><span>VERLET BUFFER <small>Condition 5</small></span><button className={`switch ${bufferEnabled ? 'on' : ''}`} onClick={() => { setBufferEnabled(!bufferEnabled); reset(); }}><span /></button></div><label className={bufferEnabled ? '' : 'muted'}>K SKIN COEFFICIENT <strong>{K}</strong></label><input disabled={!bufferEnabled} type="range" min="0" max="200" value={K} onChange={(event) => setK(Number(event.target.value))} /><div className="action-row"><button onClick={() => setPlaying(!playing)} title={playing ? 'Pause' : 'Play'}>{playing ? <Pause size={16} /> : <Play size={16} />}{playing ? 'PAUSE' : 'PLAY'}</button><button onClick={step} title="Step"><SkipForward size={16} /> STEP</button><button onClick={reset} title="Reset"><RotateCcw size={16} /></button></div></div>
      <div className="control-block stats"><div className="block-heading">RUN TELEMETRY <span>02</span></div><div className="stat-grid"><div><small>STEP</small><strong>{metrics.step.toString().padStart(5, '0')}</strong></div><div><small>LAST ΔT</small><strong>{metrics.elapsedMs.toFixed(2)}<i> ms</i></strong></div><div><small>DISTANCE CHECKS</small><strong>{metrics.distanceChecks.toLocaleString()}</strong><span className="compare">vs {latest?.distanceChecks.toLocaleString() ?? '---'}</span></div><div><small>CANDIDATE PAIRS</small><strong>{metrics.candidatePairs.toLocaleString()}</strong><span className="compare mint">{metrics.collisions} collisions</span></div></div><div className="mini-chart">{history.slice(-34).map((item, index) => <span key={`${item.step}-${index}`} style={{ height: `${Math.min(100, Math.max(8, item.distanceChecks / Math.max(metrics.distanceChecks, 1) * 100))}%` }} />)}</div><div className="footer-stats"><span>REBUILDS <b>{metrics.rebuildCount}</b></span><span>SKIPPED <b className="mint">{skippedRatio}%</b></span></div></div>
      <div className="control-block event-log"><div className="block-heading">EVENT LOG <span>03</span></div>{events.length ? events.map((event, index) => <p key={`${event}-${index}`}>{event}</p>) : <p className="quiet">Awaiting rebuild trigger...</p>}</div><button className="export" onClick={() => { const blob = new Blob([instrumentation.current.exportJSON()], { type: 'application/json' }); const link = document.createElement('a'); link.href = URL.createObjectURL(blob); link.download = 'collision-lab-run.json'; link.click(); }}><Download size={14} /> EXPORT RUN DATA</button>
    </aside>
  </main>;
}
export default App;
