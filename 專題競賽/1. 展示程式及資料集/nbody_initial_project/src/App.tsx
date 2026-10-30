import { Crosshair, Download, Languages, LocateFixed, Pause, Play, PanelRightClose, PanelRightOpen, RotateCcw, SkipBack, SkipForward, X } from 'lucide-react';
import { useEffect, useMemo, useRef, useState } from 'react';
import { OctreeStructure } from './core/spatial/OctreeStructure';
import type { Scene } from './core/ParticleSystem';
import { MAX_SHELLS } from './core/scenario';
import { SimulationRunner, type Algorithm, type SimConfig } from './core/SimulationRunner';
import type { Vec3 } from './core/types';
import { loadLang, saveLang, strings, type Lang } from './i18n';
import { COMPARE_ACCELERATION, COMPARE_CLUSTER_FACTOR, MICRO_SCRIPT, SCRIPTS, withScene, type Script, type ScriptId } from './presets';
import { FOCUS_COLOR, PAIR_PALETTE } from './view/effects';
import { COLLISION_COLOR, depthColor, PLAIN_LINE_COLOR, SELECTED_COLOR, SPECTATOR_START, TRIGGER_COLOR, type Spectator } from './view/scene';
import { createCameraSyncHub, SimulationScreen, useOctreeView, type ScreenOptions } from './view/SimulationScreen';
import { CompareStats, PanelSection, PhaseChart, recentAverage, StatsPanel } from './view/panel';

// 播放速度：1× 時每 140ms 跑一步。只改變「多久跑一步」，每一步的物理時間 dt 不變，所以結果與速度無關
const BASE_STEP_MS = 140;
const SPEEDS = [0.25, 0.5, 1, 2, 4];
// 粒子數量滑桿的刻度：從粒子少的劇本（數十顆）到大量粒子（5 萬顆）都能選
const COUNT_STEPS = [20, 30, 40, 60, 80, 100, 150, 200, 300, 500, 800, 1000, 1200, 1500, 2000, 3000, 4000, 5000, 7200, 10000, 15000, 20000, 30000, 40000, 50000];
const countIndex = (count: number) => COUNT_STEPS.reduce((best, value, index) => (Math.abs(value - count) < Math.abs(COUNT_STEPS[best] - count) ? index : best), 0);

// 進入頁面時套用劇本一（暫停在第 1 步，等觀眾按播放）
const START_SCRIPT = MICRO_SCRIPT;

// 產生新的模擬並先跑第 1 步：第 1 步一定重建，畫面一出現就看得到候選配對
const createRunner = (config: SimConfig) => { const runner = new SimulationRunner(config); runner.step(); return runner; };
// 「上一幀」最多保存幾幀：每顆粒子每幀存 13 個數字，總量控制在約 150 萬個（約 12 MB）以內，最多 300 幀
const historyFramesFor = (particles: number) => Math.max(1, Math.min(300, Math.floor(1_500_000 / (particles * 13))));
// dual（劇本二）：同一組設定產生兩份，上均勻網格、下八元樹；種子相同，所以初始粒子完全一樣
const createRunners = (config: SimConfig, dual: boolean) => {
  const list = dual ? [createRunner({ ...config, algorithm: 'Uniform Grid' }), createRunner({ ...config, algorithm: 'Octree' })] : [createRunner(config)];
  list.forEach((runner) => runner.keepHistory(historyFramesFor(config.count * list.length)));
  return list;
};

// 可收合區塊的開關狀態存在瀏覽器；無痕視窗等讀寫失敗時就用預設值
const PANEL_OPEN_KEY = 'collision-lab-panel-open';
const loadPanelOpen = (): Record<string, boolean> => {
  try { const value = JSON.parse(localStorage.getItem(PANEL_OPEN_KEY) ?? '{}'); return value && typeof value === 'object' ? value : {}; } catch { return {}; }
};

function App() {
  // config 是控制面板上的設定；滑桿拖動中的數字也先放在這裡，放開後才用它重新產生粒子
  const [config, setConfig] = useState<SimConfig>(START_SCRIPT.config);
  // dual：劇本二的上下兩個畫面
  const [dual, setDual] = useState(!!START_SCRIPT.dual);
  // 正在跑的模擬（粒子、空間結構、候選列表與數據都在裡面，見 core/SimulationRunner.ts），一般一份、劇本二兩份。
  // 只在第一次畫面時建立（寫在 useRef(...) 的括號裡會每次重新畫面都建一份再丟掉）
  const runners = useRef<SimulationRunner[] | null>(null);
  if (!runners.current) runners.current = createRunners(START_SCRIPT.config, !!START_SCRIPT.dual);
  // 劇本二兩個畫面共用的視角
  const cameraSync = useRef(createCameraSyncHub());
  // 每跑一步就 +1，讓 React 重新畫面板與 3D 畫面
  const [, setTick] = useState(0);
  // 目前套用的劇本；手動改了設定就變成 null（自由調整）
  const [activeScript, setActiveScript] = useState<ScriptId | null>(START_SCRIPT.id);
  const [pairColorsOn, setPairColorsOn] = useState(START_SCRIPT.pairColors);
  // 配色：plain（樸素，預設）只有候選配對與重點標示有顏色；rich（彩色）是原本的樣子，八元樹各層、煙火用多種顏色
  const [colorMode, setColorMode] = useState<'plain' | 'rich'>('plain');
  const plain = colorMode === 'plain';
  const [autoPause, setAutoPause] = useState(false);
  // 碰撞自動暫停後顯示提示；按播放就收起來
  const [autoPaused, setAutoPaused] = useState(false);
  // 一進頁面（或剛套用劇本）時，畫面中央顯示「按這裡開始播放」
  const [showStart, setShowStart] = useState(true);
  // 粒子雲的群聚中心數與煙火發數分開記，切換場景時各自保留
  const clusterMemory = useRef<Record<Scene, number>>({ cloud: 1, explosion: 3, micro: 1 });
  // 煙火場景的觀眾小人（位置與視線方向，每幀直接修改，不觸發重新渲染）與視角模式
  const spectator = useRef<Spectator>({ ...SPECTATOR_START });
  const [viewMode, setViewMode] = useState<'orbit' | 'person'>('orbit');
  const switchView = (mode: 'orbit' | 'person') => { setViewMode(mode); if (mode === 'orbit') setResetViewToken((value) => value + 1); };
  // 觀眾視角下滑鼠是否已鎖定（Minecraft 式視角）
  const [pointerLocked, setPointerLocked] = useState(false);
  const [showGrid, setShowGrid] = useState(false);
  // 八元樹切割只顯示選取的層級（可多選）；空集合表示顯示全部層級
  const [depthFilter, setDepthFilter] = useState<Set<number>>(() => new Set());
  const toggleDepth = (depth: number) => setDepthFilter((old) => { const next = new Set(old); if (next.has(depth)) next.delete(depth); else next.add(depth); return next; });
  const anyDepthSelected = depthFilter.size > 0;
  const [showSplits, setShowSplits] = useState(true);
  // 沒有選層級時也畫出每一層已切開的格子（劇本三）；粒子多時會太密，平常關閉
  const [allSplits, setAllSplits] = useState(false);
  const [onlyLevelParticles, setOnlyLevelParticles] = useState(false);
  const [autoRotate, setAutoRotate] = useState(false);
  const [playing, setPlaying] = useState(false);
  const [speed, setSpeed] = useState(START_SCRIPT.playbackSpeed);
  // 最近 2 秒內每一步的時間點，用來算實際每秒幾步（電腦跟不上時會低於目標）
  const stepTimes = useRef<number[]>([]);
  const [selectedId, setSelectedId] = useState<number | null>(null);
  const [focusToken, setFocusToken] = useState(0);
  const [resetViewToken, setResetViewToken] = useState(0);
  const [lang, setLang] = useState<Lang>(loadLang);
  // 側邊欄可收合區塊的開關；沒有紀錄時用各區塊的預設值
  const [panelOpen, setPanelOpen] = useState<Record<string, boolean>>(loadPanelOpen);
  // 整個側邊欄收起來，讓 3D 畫面佔滿寬度
  const [sidebarCollapsed, setSidebarCollapsed] = useState(false);
  const isOpen = (id: string, fallback = true) => panelOpen[id] ?? fallback;
  const toggleSection = (id: string, fallback = true) => setPanelOpen((old) => {
    const next = { ...old, [id]: !(old[id] ?? fallback) };
    try { localStorage.setItem(PANEL_OPEN_KEY, JSON.stringify(next)); } catch { /* 無痕視窗等情況下不保存，但照常切換 */ }
    return next;
  });
  const t = strings[lang];
  useEffect(() => { document.documentElement.lang = lang === 'zh' ? 'zh-Hant' : 'en'; saveLang(lang); }, [lang]);
  // 方法名稱：中文介面寫成「八元樹（Octree）」，中英並列
  const methodName = (item: Algorithm) => (t.algorithmSub[item] ? `${t.algorithms[item]}（${t.algorithmSub[item]}）` : t.algorithms[item]);

  // 用 next 這組設定重新產生粒子（不傳就用目前面板上的設定）
  const reset = (next: SimConfig = config, nextDual = dual) => {
    runners.current = createRunners(next, nextDual);
    setSelectedId(null);
    setDepthFilter(new Set());
    setAutoPaused(false);
    setTick((value) => value + 1);
  };
  // 改設定並立刻重新產生粒子（切換場景、空間結構、Verlet 開關、聚集數量）。
  // 手動改過就不再是劇本的設定（keepScript：劇本二畫面上的控制項本來就是劇本的一部分）；
  // 選了某一種方法就離開上下雙畫面
  const applyConfig = (patch: Partial<SimConfig>, keepScript = false) => {
    const next = { ...config, ...patch };
    const nextDual = patch.algorithm ? false : dual;
    setConfig(next);
    setDual(nextDual);
    if (!keepScript) setActiveScript(null);
    reset(next, nextDual);
  };
  // 滑桿放開（滑鼠、觸控或鍵盤）才重新產生粒子；手機上拖動滑桿不會觸發 mouseup，所以三種都要聽。設定沒變就不重來
  const commitSliders = () => { if (JSON.stringify(config) !== JSON.stringify(runners.current![0].config)) { setActiveScript(null); reset(config); } };
  const sliderCommit = { onPointerUp: commitSliders, onTouchEnd: commitSliders, onKeyUp: commitSliders };
  // 一鍵套用劇本：換設定、重新產生粒子、調好播放速度與顯示選項，暫停在第 1 步等觀眾按播放
  const applyScript = (script: Script) => {
    setConfig(script.config);
    setDual(!!script.dual);
    reset(script.config, !!script.dual);
    setActiveScript(script.id);
    setSpeed(script.playbackSpeed);
    setPairColorsOn(script.pairColors);
    setShowGrid(!!script.showOctree);
    setAllSplits(!!script.allSplits);
    setColorMode(script.colorMode ?? 'plain');
    setAutoRotate(!!script.autoRotate);
    setViewMode('orbit');
    setPlaying(false);
    setShowStart(true);
    setResetViewToken((value) => value + 1);
  };
  const togglePlay = () => { setPlaying(!playing); setShowStart(false); setAutoPaused(false); };
  // 更新聚集數量（1～MAX_SHELLS），並以新的數量重新產生粒子
  const applyClusters = (value: number) => {
    if (!Number.isFinite(value)) return;
    const next = Math.min(MAX_SHELLS, Math.max(1, Math.round(value)));
    clusterMemory.current[config.scene] = next;
    applyConfig({ clusters: next });
  };
  // K 改了不必重新產生粒子：模擬下一步直接用新的 K 重建（劇本二兩邊一起改）
  const changeK = (K: number, keepScript = false) => {
    setConfig({ ...config, K });
    if (!keepScript) setActiveScript(null);
    runners.current!.forEach((runner) => runner.setK(K));
    setTick((value) => value + 1);
  };

  // 跑一步（實際流程在 SimulationRunner.step()），再讓畫面更新；劇本二兩份一起前進
  const step = () => {
    const started = performance.now();
    stepTimes.current = [...stepTimes.current.filter((time) => started - time < 2000), started];
    const all = runners.current!;
    all.forEach((runner) => runner.step());
    if (autoPause && all.some((runner) => runner.newCollisionPairs.length)) { setPlaying(false); setAutoPaused(true); }
    setTick((value) => value + 1);
  };
  // 下一幀：先暫停，再前進一步
  const nextFrame = () => { setPlaying(false); setShowStart(false); setAutoPaused(false); step(); };
  // 上一幀：先暫停，再回到上一步之前的狀態（劇本二兩份一起退）
  const prevFrame = () => {
    setPlaying(false); setShowStart(false); setAutoPaused(false);
    runners.current!.forEach((runner) => runner.stepBack());
    setTick((value) => value + 1);
  };
  useEffect(() => { if (!playing) return; const timer = window.setInterval(step, BASE_STEP_MS / speed); return () => window.clearInterval(timer); });

  const all = runners.current!;
  // 側邊欄的數據、粒子資訊與重建記錄以第一份模擬（劇本二是上方的均勻網格）為準
  const sim = all[0];
  const { metrics, events } = sim;
  const history = sim.instrumentation.getHistory();
  const { algorithm, scene, bufferEnabled } = sim.config;
  // 八元樹層級清單用八元樹那一份（劇本二是下方畫面）
  const octreeRunner = all.find((runner) => runner.config.algorithm === 'Octree') ?? sim;
  const octree = useOctreeView(octreeRunner, showGrid, depthFilter, showSplits, onlyLevelParticles, allSplits);
  const triggerId = bufferEnabled && events.length ? events[0].triggeredByParticleId : null;
  const selected = selectedId !== null ? sim.particles[selectedId] ?? null : null;
  const selectParticle = (id: number, focus = false) => { setSelectedId(id); if (focus) setFocusToken((value) => value + 1); };
  // 葉節點只在重建時改變，所以依 rebuildCount 重新查詢
  const selectedLeaf = useMemo(() => (selected && sim.structure instanceof OctreeStructure ? sim.structure.getLeafOf(selected.id) : null),
    [sim, selectedId, metrics.rebuildCount]); // eslint-disable-line react-hooks/exhaustive-deps
  const skinCap = !selected ? null : algorithm === 'Octree' ? (selectedLeaf ? Math.max(selectedLeaf.halfExtent - selected.radius, 0) : null) : algorithm === 'Uniform Grid' ? Math.max(sim.cellSize / 2 - selected.radius, 0) : null;
  const sceneName = scene === 'explosion' ? t.sceneExplosion(sim.config.clusters) : scene === 'micro' ? t.sceneMicro(sim.config.count) : sim.config.clusterFactor > 0 ? t.spatialCluster(sim.config.clusterFactor.toFixed(2), sim.config.clusters) : t.uniformCloud;
  const screenOptions: ScreenOptions = { pairColorsOn, plain, showGrid, depthFilter, showSplits, allSplits, onlyLevelParticles, autoRotate, selectedId, onSelect: (id) => selectParticle(id), focusToken, resetViewToken, wide: dual, viewMode, spectator: spectator.current, onLockChange: setPointerLocked };

  return (
    <main className={`app-shell ${sidebarCollapsed ? 'sidebar-collapsed' : ''}`}>
      <section className="viewport">
        {/* 模擬畫面：像電視螢幕一樣加外框，上方標題列寫清楚目前的方法與設定，圖例放在畫面下方，不擋住粒子 */}
        <div className="screen">
          <div className="screen-titlebar">
            <span className="screen-title"><span className="live-dot" />{t.screenTitle}</span>
            <span className="screen-status">
              <span>{t.statusMethod}<b>{dual ? `${t.viewTop} ${methodName(all[0].config.algorithm)}／${t.viewBottom} ${methodName(all[1].config.algorithm)}` : methodName(algorithm)}</b></span>
              <span>{t.statusVerlet}<b>{bufferEnabled ? t.verletOn(sim.config.K) : t.verletOff}</b></span>
              <span>{t.statusScene}<b>{sceneName}</b></span>
            </span>
            <button className="sidebar-toggle" onClick={() => setSidebarCollapsed(!sidebarCollapsed)} title={sidebarCollapsed ? t.showSidebar : t.hideSidebar} aria-label={sidebarCollapsed ? t.showSidebar : t.hideSidebar}>
              {sidebarCollapsed ? <PanelRightOpen size={16} /> : <PanelRightClose size={16} />}
            </button>
          </div>
          <div className={`screen-body ${dual ? 'dual' : ''}`}>
            {all.map((runner, index) => (
              <SimulationScreen key={index} runner={runner} options={screenOptions} cameraSync={dual ? cameraSync.current : undefined} primary={index === 0}>
                {dual && (
                  <div className="view-tag">
                    <b>{index === 0 ? t.viewTop : t.viewBottom}：{methodName(runner.config.algorithm)}</b>
                    <span>{t.miniStats(runner.metrics.rebuildCount, runner.metrics.candidatePairs, recentAverage(runner.instrumentation.getHistory(), 'detectionMs'))}</span>
                  </div>
                )}
                {runner.lastRebuild && <div className="rebuild-banner" key={runner.lastRebuild.step}>{t.rebuildBanner(runner.lastRebuild.rebuildCount)}<small>{t.rebuildBannerDetail(runner.lastRebuild.triggerId, runner.lastRebuild.detectionMs)}</small></div>}
              </SimulationScreen>
            ))}
            {scene === 'explosion' && viewMode === 'person' && (
              <>
                <div className="crosshair" />
                {!pointerLocked && <div className="lock-hint">{t.lockHint}</div>}
              </>
            )}
            {/* 第一步該做什麼：畫面中央的大按鈕，按了就開始播放 */}
            {showStart && !playing && (
              <div className="start-overlay">
                <button onClick={togglePlay}>
                  <Play size={30} />
                  <span>{t.startTitle}</span>
                  {activeScript && <small>{t.scriptHints[activeScript]}</small>}
                </button>
              </div>
            )}
            {autoPaused && !playing && <div className="pause-note">{t.autoPausedNote}</div>}
          </div>
          {/* 劇本二：skin 參數與場景開關直接放在畫面上，可以邊看邊調 */}
          {dual && (
            <div className="screen-controls">
              <label className="k-control">
                <span>{t.onScreenK} <b>{config.K}</b></span>
                <input type="range" min="1" max="60" value={config.K} onChange={(event) => changeK(Number(event.target.value), true)} />
              </label>
              <div className="inline-toggle">
                <span>{t.onScreenDistribution}</span>
                <div className="mini-segmented">
                  {[0, COMPARE_CLUSTER_FACTOR].map((factor) => (
                    <button key={factor} className={config.clusterFactor === factor ? 'active' : ''} onClick={() => applyConfig({ clusterFactor: factor }, true)}>{factor ? t.distributionClustered : t.distributionUniform}</button>
                  ))}
                </div>
              </div>
              <div className="inline-toggle">
                <span>{t.onScreenAcceleration}</span>
                <div className="mini-segmented">
                  {[COMPARE_ACCELERATION, 0].map((acceleration) => (
                    <button key={acceleration} className={config.acceleration === acceleration ? 'active' : ''} onClick={() => applyConfig({ acceleration }, true)}>{acceleration ? t.accelerationOn : t.accelerationOff}</button>
                  ))}
                </div>
              </div>
            </div>
          )}
          <div className="screen-legend">
            {pairColorsOn && <span><i style={{ background: `linear-gradient(90deg, ${PAIR_PALETTE.slice(0, 4).join(', ')})` }} />{t.legendPairs}</span>}
            {sim.system.focusIds.length > 0 && <span><i className="ring" style={{ borderColor: FOCUS_COLOR }} />{t.legendFocus}</span>}
            <span><i style={{ background: COLLISION_COLOR }} />{t.legendCollision}</span>
            <span><i style={{ background: TRIGGER_COLOR }} />{t.legendTrigger}</span>
            <span><i style={{ background: SELECTED_COLOR }} />{t.legendSelected}</span>
            {bufferEnabled && <span className="rebuild-note">{t.rebuildAllNote}</span>}
            {dual && <span className="rebuild-note">{t.sameMotionNote}</span>}
            <span className="hint">{t.clickToSelect} · {scene === 'explosion' ? (viewMode === 'person' ? t.keyboardHintPerson : t.keyboardHintSpectator) : t.keyboardHint}</span>
          </div>
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
          </div>
        </header>

        {/* 劇本：一鍵套用預先調好的設定 */}
        <div className="script-bar">
          <div className="block-heading">{t.scriptsTitle}</div>
          <div className="script-buttons">
            {SCRIPTS.map((script) => (
              <button key={script.id} className={activeScript === script.id ? 'active' : ''} onClick={() => applyScript(script)}>
                {t.scriptNames[script.id]}
                <small>{t.scriptHints[script.id]}</small>
              </button>
            ))}
          </div>
        </div>

        {/* 播放列：固定在側邊欄頂端，捲動時也按得到 */}
        <div className="playbar">
          <div className="action-row">
            <button onClick={togglePlay} title={playing ? t.pause : t.play}>
              {playing ? <Pause size={16} /> : <Play size={16} />}
              {playing ? t.pause : t.play}
            </button>
            <button onClick={prevFrame} disabled={!sim.canStepBack} title={t.prevFrameHint(historyFramesFor(sim.config.count * all.length))}>
              <SkipBack size={16} /> {t.prevFrame}
            </button>
            <button onClick={nextFrame} title={t.stepHint}>
              <SkipForward size={16} /> {t.step}
            </button>
            <button onClick={() => reset()} title={t.reset} aria-label={t.reset}>
              <RotateCcw size={16} />
            </button>
          </div>
          <div className="speed-row">
            <span>{t.speed} <small>{t.speedHint(Math.round(1000 / (BASE_STEP_MS / speed)), playing ? stepTimes.current.filter((time) => performance.now() - time < 2000).length / 2 : 0)}</small></span>
            <div className="speed-buttons">
              {SPEEDS.map((value) => (
                <button key={value} className={speed === value ? 'active' : ''} onClick={() => setSpeed(value)}>{value}×</button>
              ))}
            </div>
          </div>
          <label className="check-row">
            <input type="checkbox" checked={autoPause} onChange={(event) => setAutoPause(event.target.checked)} />
            {t.autoPause}
          </label>
        </div>

        <PanelSection index="01" title={t.statsTitle} className="stats">
          {dual
            ? <CompareStats columns={all.map((runner) => ({ name: methodName(runner.config.algorithm), metrics: runner.metrics, history: runner.instrumentation.getHistory() }))} t={t} />
            : <StatsPanel metrics={metrics} history={history} t={t} />}
        </PanelSection>

        <PanelSection index="02" title={t.sectionDisplay} hint={t.sectionDisplayHint}>
          <div className="toggle-row first">
            <span>{t.pairColors} <small>{t.pairColorsHint}</small></span>
            <button className={`switch ${pairColorsOn ? 'on' : ''}`} onClick={() => setPairColorsOn(!pairColorsOn)} aria-label={t.pairColors}>
              <span />
            </button>
          </div>
          <label className="check-row">
            <input type="checkbox" checked={autoRotate} onChange={(event) => setAutoRotate(event.target.checked)} />
            {t.autoRotate}
          </label>
          <div className="toggle-row">
            <span>{t.colorMode} <small>{t.colorModeHint}</small></span>
            <div className="mini-segmented">
              {(['plain', 'rich'] as const).map((mode) => (
                <button key={mode} className={colorMode === mode ? 'active' : ''} onClick={() => setColorMode(mode)}>{mode === 'plain' ? t.colorPlain : t.colorRich}</button>
              ))}
            </div>
          </div>
          {(algorithm === 'Uniform Grid' || algorithm === 'Octree') ? (
            <>
              <div className="toggle-row">
                <span>{dual ? `${t.showGrid}／${t.showOctree}` : algorithm === 'Octree' ? t.showOctree : t.showGrid}</span>
                <button className={`switch ${showGrid ? 'on' : ''}`} onClick={() => setShowGrid(!showGrid)} aria-label={algorithm === 'Octree' ? t.showOctree : t.showGrid}>
                  <span />
                </button>
              </div>
              {octreeRunner.config.algorithm === 'Octree' && showGrid && (
                <div className="depth-list">
                  <div className="depth-list-head"><span>{t.depthLevel}</span><span>{t.depthSize}</span><span>{t.depthLeaves}</span><span>{t.depthSplits}</span></div>
                  <button className={!anyDepthSelected ? 'active' : ''} onClick={() => setDepthFilter(new Set())}>
                    <span><i className={plain ? 'plain' : 'all'} />{t.depthAll}</span><span /><span>{octree.geometry.boxes.length}</span><span>{octree.geometry.splits.length}</span>
                  </button>
                  {octree.depthStats.map(({ depth, size, count, splitCount }) => (
                    <button key={depth} className={depthFilter.has(depth) ? 'active' : ''} onClick={() => toggleDepth(depth)}>
                      <span><i style={{ background: plain ? PLAIN_LINE_COLOR : depthColor(depth) }} />{t.depthName(depth)}</span><span>{size.toFixed(3)}</span><span>{count}</span><span className="split-count">{splitCount}</span>
                    </button>
                  ))}
                  <label className="split-toggle">
                    <input type="checkbox" checked={allSplits} onChange={(event) => setAllSplits(event.target.checked)} />
                    <span><i className="dash" />{t.allSplits}</span>
                  </label>
                  <label className={`split-toggle ${!anyDepthSelected ? 'muted' : ''}`}>
                    <input type="checkbox" checked={showSplits} disabled={!anyDepthSelected} onChange={(event) => setShowSplits(event.target.checked)} />
                    <span><i className="dash" />{t.showSplits}</span>
                  </label>
                  <label className={`split-toggle ${!anyDepthSelected ? 'muted' : ''}`}>
                    <input type="checkbox" checked={onlyLevelParticles} disabled={!anyDepthSelected} onChange={(event) => setOnlyLevelParticles(event.target.checked)} />
                    <span><i className="dot" />{!anyDepthSelected ? t.onlyLevelParticles : t.onlyLevelParticlesCount(octree.particleCountAtDepth)}</span>
                  </label>
                  <p className="depth-note">{!anyDepthSelected ? t.depthNote : t.depthNoteSelected}</p>
                </div>
              )}
            </>
          ) : <p className="block-note">{t.displayNoGrid}</p>}
          {scene === 'explosion' && (
            <>
              <label>{t.viewMode}</label>
              <div className="segmented">
                {(['orbit', 'person'] as const).map((mode) => (
                  <button key={mode} className={viewMode === mode ? 'active' : ''} onClick={() => switchView(mode)}>{mode === 'orbit' ? t.viewOrbit : t.viewPerson}</button>
                ))}
              </div>
              <button className="spectator-reset" onClick={() => { Object.assign(spectator.current, SPECTATOR_START); }}>{t.spectatorReset}</button>
            </>
          )}
        </PanelSection>

        <PanelSection index="03" title={t.inspector} className="inspector" collapsible open={isOpen('inspector')} onToggle={() => toggleSection('inspector')} badge={selected ? `#${selected.id}` : undefined} t={t}>
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
            const { leafCapacity, maxDepth } = sim.config;
            const rows: [string, string, string?][] = [
              [t.fieldId, `#${selected.id}`],
              [t.fieldStatus, sim.collisionIds.has(selected.id) ? t.statusColliding : sim.candidateIds.has(selected.id) ? t.statusCandidate : t.statusFree, sim.collisionIds.has(selected.id) ? 'hot' : undefined],
              [t.fieldPosition, vec(selected.position)],
              [t.fieldVelocity, `${speed.toFixed(3)}  ${vec(selected.velocity)}`],
              [t.fieldAcceleration, `${acc.toFixed(3)}  ${vec(selected.acceleration)}`],
              [t.fieldRadius, selected.radius.toFixed(3)],
              [t.fieldSkin, bufferEnabled ? selected.skin.toFixed(4) : t.notApplicable],
              [t.fieldSkinCap, skinCap === null ? t.notApplicable : `${skinCap.toFixed(4)}${bufferEnabled && skinCap > 0 && selected.skin >= skinCap - 1e-9 ? `  ${t.capped}` : ''}`],
              [t.fieldDisplacement, bufferEnabled ? `${dx.toFixed(4)}  (${selected.skin > 0 ? `${((dx / selected.skin) * 100).toFixed(0)}%` : '—'} ${t.ofSkin})` : t.notApplicable, bufferEnabled && dx > selected.skin ? 'hot' : undefined],
              [t.fieldLeaf, selectedLeaf ? t.leafInfo(selectedLeaf.depth, selectedLeaf.halfExtent) : algorithm === 'Uniform Grid' ? t.gridCell(sim.cellSize) : t.notApplicable],
              [t.fieldCapacity, selectedLeaf ? (selectedLeaf.depth >= maxDepth ? t.capacityUnlimited(selectedLeaf.count, maxDepth) : t.capacityOf(selectedLeaf.count, leafCapacity)) : algorithm === 'Uniform Grid' ? t.capacityGrid : t.notApplicable,
                selectedLeaf && selectedLeaf.count > leafCapacity ? 'hot' : undefined],
              [t.fieldTriggerCount, String(sim.triggerCounts.get(selected.id) ?? 0)],
            ];
            return <dl className="inspector-grid">{rows.map(([label, value, tone]) => <div key={label}><dt>{label}</dt><dd className={tone ?? ''}>{value}</dd></div>)}</dl>;
          })() : <p className="quiet">{t.inspectorEmpty}</p>}
        </PanelSection>

        <PanelSection index="04" title={t.sectionFree} hint={isOpen('free', false) ? t.sectionFreeHint : undefined} className="free-block" collapsible open={isOpen('free', false)} onToggle={() => toggleSection('free', false)} t={t}>
          <label>{t.sceneLabel}</label>
          <div className="segmented three">
            {(['micro', 'cloud', 'explosion'] as Scene[]).map((item) => (
              <button className={config.scene === item ? 'active' : ''} onClick={() => { setViewMode('orbit'); applyConfig(withScene(config, item, clusterMemory.current[item])); setResetViewToken((value) => value + 1); }} key={item}>
                {t.scenes[item]}
              </button>
            ))}
          </div>

          <label>
            {t.particleCount} <strong>{config.count.toLocaleString()}</strong>
          </label>
          <input type="range" min={0} max={COUNT_STEPS.length - 1} step={1} value={countIndex(config.count)} onChange={(event) => setConfig({ ...config, count: COUNT_STEPS[Number(event.target.value)] })} {...sliderCommit} />
          <div className="range-endpoints"><span>{COUNT_STEPS[0]}</span><span>{COUNT_STEPS.at(-1)!.toLocaleString()}</span></div>

          {/* 群聚程度只對粒子雲有意義；群聚中心數只在群聚程度大於 0 時才看得出差別 */}
          {config.scene === 'cloud' && (
            <>
              <label>
                <span>{t.clusterFactor} <small>{t.clusterFactorHint}</small></span> <strong>{config.clusterFactor.toFixed(2)}</strong>
              </label>
              <input type="range" min="0" max="1" step="0.01" value={config.clusterFactor} onChange={(event) => setConfig({ ...config, clusterFactor: Number(event.target.value) })} {...sliderCommit} />
              <div className="range-endpoints"><span>0.00</span><span>1.00</span></div>
            </>
          )}
          {(config.scene === 'explosion' || (config.scene === 'cloud' && config.clusterFactor > 0)) && (
            <div className="count-row">
              <span>{config.scene === 'explosion' ? t.shellCount : t.clusterCount}<small>{config.scene === 'explosion' ? t.shellCountHint : t.clusterCountHint}</small></span>
              <div className="stepper">
                <button onClick={() => applyClusters(config.clusters - 1)} disabled={config.clusters <= 1} aria-label="-1">−</button>
                <input type="number" min={1} max={MAX_SHELLS} value={config.clusters} onChange={(event) => applyClusters(Number(event.target.value))} />
                <button onClick={() => applyClusters(config.clusters + 1)} disabled={config.clusters >= MAX_SHELLS} aria-label="+1">+</button>
              </div>
            </div>
          )}

          {/* 小空間是等速運動，沒有加速度 */}
          {config.scene !== 'micro' && (
            <>
              <label>
                {config.scene === 'explosion' ? t.gravity : t.acceleration} <strong>{config.scene === 'explosion' ? '' : '±'}{config.acceleration.toFixed(2)}</strong>
              </label>
              <input type="range" min="0" max="2" step="0.05" value={config.acceleration} onChange={(event) => setConfig({ ...config, acceleration: Number(event.target.value) })} {...sliderCommit} />
              <div className="range-endpoints"><span>0.00</span><span>2.00</span></div>
            </>
          )}

          <label>{t.structureLabel}</label>
          <div className="segmented three">
            {(['Brute Force', 'Uniform Grid', 'Octree'] as Algorithm[]).map((item) => (
              <button className={!dual && config.algorithm === item ? 'active' : ''} onClick={() => applyConfig({ algorithm: item })} key={item}>
                {t.algorithms[item]}
                {t.algorithmSub[item] && <small>{t.algorithmSub[item]}</small>}
              </button>
            ))}
          </div>

          <div className="toggle-row">
            <span>{t.verletBuffer} <small>{t.verletBufferHint}</small></span>
            <button className={`switch ${config.bufferEnabled ? 'on' : ''}`} onClick={() => applyConfig({ bufferEnabled: !config.bufferEnabled })} aria-label={t.verletBuffer}>
              <span />
            </button>
          </div>
          {config.bufferEnabled ? (
            <>
              <label>
                <span>{t.kSkin} <small>{t.kSkinHint}</small></span> <strong>{config.K}</strong>
              </label>
              <input type="range" min="0" max="200" value={config.K} onChange={(event) => changeK(Number(event.target.value))} />
            </>
          ) : <p className="block-note">{t.kSkinOffHint}</p>}
        </PanelSection>

        <PanelSection index="05" title={t.eventLog} className="event-log" collapsible open={isOpen('events', false)} onToggle={() => toggleSection('events', false)} badge={events.length ? String(events.length) : undefined} t={t}>
          <div className="event-list">
            {events.length ? events.map((event, index) => (
              <button className={`event-item ${event.triggeredByParticleId === selectedId ? 'selected' : ''}`} key={`${event.step}-${index}`} title={t.eventClickHint}
                onClick={() => selectParticle(event.triggeredByParticleId, true)}>
                {t.rebuildEvent(event)}
              </button>
            )) : <p className="quiet">{bufferEnabled ? t.awaitingRebuild : t.bufferOffNoEvents}</p>}
          </div>
        </PanelSection>

        <PanelSection index="06" title={t.sectionAdvanced} className="phase-block" collapsible open={isOpen('advanced', false)} onToggle={() => toggleSection('advanced', false)} t={t}>
          <div className="phase-chart-title mint">{t.broadPhaseTitle}</div>
          <PhaseChart history={history} metricKey="broadPhaseMs" color="#5DCAA5" label={t.broadPhase} markRebuilds={bufferEnabled} t={t} />
          <div className="phase-chart-title gold">{t.narrowPhaseTitle}</div>
          <PhaseChart history={history} metricKey="narrowPhaseMs" color="#D9A441" label={t.narrowPhase} t={t} />
          <button className="export" onClick={() => { const blob = new Blob([sim.instrumentation.exportJSON()], { type: 'application/json' }); const link = document.createElement('a'); link.href = URL.createObjectURL(blob); link.download = 'collision-lab-run.json'; link.click(); }}>
            <Download size={14} /> {t.exportRunData}
          </button>
        </PanelSection>
      </aside>
    </main>
  );
}

export default App;
