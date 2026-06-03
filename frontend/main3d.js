import * as THREE from "https://cdn.jsdelivr.net/npm/three@0.160.0/build/three.module.js";
import { OrbitControls } from "https://cdn.jsdelivr.net/npm/three@0.160.0/examples/jsm/controls/OrbitControls.js";

(() => {
  // ======= CONFIG FOR EACH VIEW =======
  const VIEW_CONFIGS = [
    { id: 0, label: "Naive", mode: "naive", cellSize: 40 },
    { id: 1, label: "Grid Small", mode: "grid", cellSize: 40 },
    { id: 2, label: "Grid Medium", mode: "grid", cellSize: 60 },
    { id: 3, label: "Grid Large", mode: "grid", cellSize: 100 },
  ];

  // ======= SHARED CONSTANTS =======
  const G = 35;
  const softening = 25;
  const dt = 0.016;
  const damping = 0.997;
  const BOX_LIMIT = 380;

  // ======= SHARED STATE =======
  let particles = [];
  let paused = false;
  let selectedIndex = -1;
  let timeScale = 1;
  let energyLoss = 0;
  let energyFrameCounter = 0;
  let isAdmin = false;
  let collisionMode = "merge";
  let activeViewIndex = 0;
  const ADMIN_PASSWORD = "123";

  // ======= SHARED UI REFERENCES =======
  const scenarioSel = document.getElementById("scenario");
  const nRange = document.getElementById("nRange");
  const nLabel = document.getElementById("nLabel");
  const cellRange = document.getElementById("cellRange");
  const cellLabel = document.getElementById("cellLabel");
  const randomSizeMass = document.getElementById("randomSizeMass");
  const activeViewSel = document.getElementById("activeViewSel");
  const activeViewMode = document.getElementById("activeViewMode");
  const activeCellRange = document.getElementById("activeCellRange");
  const activeCellLabel = document.getElementById("activeCellSizeLabel");
  const modeSel = document.getElementById("mode");
  const collisionModeSel = document.getElementById("collisionMode");
  const speedRange = document.getElementById("speedRange");
  const speedLabel = document.getElementById("speedLabel");
  const lossRange = document.getElementById("lossRange");
  const lossLabel = document.getElementById("lossLabel");
  const speedLimitNotice = document.getElementById("speedLimitNotice");
  const resetBtn = document.getElementById("resetBtn");
  const pauseBtn = document.getElementById("pauseBtn");
  const msEl = document.getElementById("ms");
  const opsEl = document.getElementById("ops");
  const eiEl = document.getElementById("ei");
  const tcEl = document.getElementById("tc");
  const energyEl = document.getElementById("energy");
  const addPlanetBtn = document.getElementById("addPlanetBtn");
  const addX = document.getElementById("addX");
  const addY = document.getElementById("addY");
  const addZ = document.getElementById("addZ");
  const addVx = document.getElementById("addVx");
  const addVy = document.getElementById("addVy");
  const addVz = document.getElementById("addVz");
  const addR = document.getElementById("addR");
  const addRho = document.getElementById("addRho");
  const editorBox = document.getElementById("editorBox");
  const selName = document.getElementById("selName");
  const rInput = document.getElementById("rInput");
  const rhoInput = document.getElementById("rhoInput");
  const mInput = document.getElementById("mInput");
  const rVal = document.getElementById("rVal");
  const rhoVal = document.getElementById("rhoVal");
  const mVal = document.getElementById("mVal");
  const colorInput = document.getElementById("colorInput");
  const brightInput = document.getElementById("brightInput");
  const brightVal = document.getElementById("brightVal");
  const planetListToggleBtn = document.getElementById("planetListToggleBtn");
  const planetListBody = document.getElementById("planetListBody");
  const planetCountEl = document.getElementById("planetCount");
  const planetListEl = document.getElementById("planetList");
  const addColor = document.getElementById("addColor");
  const addBright = document.getElementById("addBright");
  const loginBtn = document.getElementById("loginBtn");
  const logoutBtn = document.getElementById("logoutBtn");
  const loginStatus = document.getElementById("loginStatus");
  const modalLogin = document.getElementById("modalLogin");
  const adminPassword = document.getElementById("adminPassword");
  const submitLogin = document.getElementById("submitLogin");
  const cancelLogin = document.getElementById("cancelLogin");
  const closeLoginModal = document.getElementById("closeLoginModal");
  const panelContainer = document.getElementById("panelContainer");

  const adminElements = [
    "scenario", "nRange", "nLabel", "cellRange", "cellLabel", "lossRange", "lossLabel",
    "resetBtn", "addPlanetBtn", "addX", "addY", "addZ", "addVx", "addVy", "addVz", "addR", "addRho", "addColor", "addBright",
    "rInput", "rhoInput", "mInput", "colorInput", "brightInput", "editorBox"
  ];

  // ======= VIEWS MANAGEMENT =======
  const views = [];
  const panelVisibility = [true, true, true, true];

  class View3D {
    constructor(config) {
      this.config = config;
      this.id = config.id;
      this.container = document.getElementById(`view3d-${config.id}`);
      this.renderer = new THREE.WebGLRenderer({ antialias: true });
      this.scene = new THREE.Scene();
      this.scene.background = new THREE.Color(0xe6e6e6);
      this.scene.fog = null;
      this.camera = new THREE.PerspectiveCamera(60, 1, 0.1, 5000);
      this.camera.position.set(0, 200, 400);
      this.controls = new OrbitControls(this.camera, this.renderer.domElement);
      this.controls.enableDamping = true;
      this.controls.enablePan = true;
      this.controls.screenSpacePanning = true;
      this.controls.minDistance = 30;
      this.controls.maxDistance = 1400;

      this.renderer.setPixelRatio(window.devicePixelRatio || 1);
      this.container.appendChild(this.renderer.domElement);

      this.scene.add(new THREE.AmbientLight(0xffffff, 0.7));
      const dir = new THREE.DirectionalLight(0xffffff, 0.8);
      dir.position.set(200, 400, 300);
      this.scene.add(dir);

      this.spheres = [];
      this.trailLines = [];
      this.gridHelper = null;
      this.gridCellFillMesh = null;
      this.gridCellHighlightMesh = null;
      this.gridCellCapacity = 0;

      this.raycaster = new THREE.Raycaster();
      this.mouse = new THREE.Vector2();

      const gridSparseColor = new THREE.Color(0xc7d2df);
      const gridDenseColor = new THREE.Color(0x34587c);
      const gridNeighborColor = new THREE.Color(0xffc04d);
      const gridSelectedCellColor = new THREE.Color(0xff7a00);
      this.gridColors = { gridSparseColor, gridDenseColor, gridNeighborColor, gridSelectedCellColor };

      this.tmpGridColor = new THREE.Color();
      this.tmpGridScale = new THREE.Vector3();
      this.tmpGridPosition = new THREE.Vector3();
      this.tmpGridMatrix = new THREE.Matrix4();
      this.identityQuat = new THREE.Quaternion();

      this.baseMat = new THREE.MeshStandardMaterial({ color: 0x000000 });
      this.selMat = new THREE.MeshStandardMaterial({ color: 0xffb020 });

      this.lastNaiveOpsBaseline = null;
      this.lastNForBaseline = null;
      this.lastOps = 0;

      this.container.addEventListener("pointerdown", (e) => this.onPointerDown(e));
      window.addEventListener("resize", () => this.resize());
      this.resize();
    }

    getGridInfo(cellSize) {
      const span = BOX_LIMIT * 2;
      const nx = Math.max(1, Math.floor(span / cellSize));
      const ny = Math.max(1, Math.floor(span / cellSize));
      const nz = Math.max(1, Math.floor(span / cellSize));
      return {
        cellSize, nx, ny, nz,
        key: (cx, cy, cz) => `${cx},${cy},${cz}`,
        idOf: (cx, cy, cz) => cx + cy * nx + cz * nx * ny,
        toCellIndex: (v, n) => Math.min(n - 1, Math.max(0, Math.floor((v + BOX_LIMIT) / cellSize))),
        centerOf: (cx, cy, cz) => ({
          x: -BOX_LIMIT + (cx + 0.5) * cellSize,
          y: -BOX_LIMIT + (cy + 0.5) * cellSize,
          z: -BOX_LIMIT + (cz + 0.5) * cellSize,
        }),
      };
    }

    buildGridBuckets(cellSize) {
      const info = this.getGridInfo(cellSize);

      // Optional micro-benchmark flag (set to true temporarily to profile)
      const BENCH = false;
      if (BENCH) console.time("buildGridBuckets");

      const N = particles.length;
      // First pass: count particles per cellId (numeric id)
      const counts = new Map();
      const particleCellId = new Array(N);

      for (let i = 0; i < N; i++) {
        const p = particles[i];
        const cx = info.toCellIndex(p.x, info.nx);
        const cy = info.toCellIndex(p.y, info.ny);
        const cz = info.toCellIndex(p.z, info.nz);
        const id = info.idOf(cx, cy, cz);
        particleCellId[i] = id;
        counts.set(id, (counts.get(id) || 0) + 1);
      }

      // If there are no occupied cells, return empty Map
      if (counts.size === 0) {
        if (BENCH) console.timeEnd("buildGridBuckets");
        return { info, buckets: new Map() };
      }

      // Build starts (prefix-sum) over the list of occupied cell ids
      const cellIds = Array.from(counts.keys());
      const starts = new Map();
      let start = 0;
      for (const id of cellIds) {
        starts.set(id, start);
        start += counts.get(id);
      }

      // Fill particleMap using starts as current offsets
      const particleMap = new Array(N);
      const currentOffset = new Map(starts);
      for (let i = 0; i < N; i++) {
        const id = particleCellId[i];
        const off = currentOffset.get(id);
        particleMap[off] = i;
        currentOffset.set(id, off + 1);
      }

      // Build buckets Map with compact indices arrays (slice of particleMap)
      const buckets = new Map();
      for (const id of cellIds) {
        const cx = id % info.nx;
        const tmp = Math.floor(id / info.nx);
        const cy = tmp % info.ny;
        const cz = Math.floor(tmp / info.ny);
        const k = info.key(cx, cy, cz);
        const s = starts.get(id);
        const c = counts.get(id);
        const indices = particleMap.slice(s, s + c);
        buckets.set(k, { key: k, cx, cy, cz, indices });
      }

      if (BENCH) console.timeEnd("buildGridBuckets");
      return { info, buckets };
    }

    rebuildGridHelper() {
      if (this.gridHelper) {
        this.scene.remove(this.gridHelper);
        this.gridHelper.traverse((obj) => {
          if (obj.geometry) obj.geometry.dispose();
          if (obj.material) obj.material.dispose();
        });
      }

      this.gridHelper = new THREE.Group();
      const cellSize = Math.max(10, this.config.cellSize);
      const half = BOX_LIMIT;
      const positions = [];

      for (let y = -half; y <= half + 0.001; y += cellSize) {
        for (let z = -half; z <= half + 0.001; z += cellSize) {
          positions.push(-half, y, z, half, y, z);
        }
      }
      for (let x = -half; x <= half + 0.001; x += cellSize) {
        for (let z = -half; z <= half + 0.001; z += cellSize) {
          positions.push(x, -half, z, x, half, z);
        }
      }
      for (let x = -half; x <= half + 0.001; x += cellSize) {
        for (let y = -half; y <= half + 0.001; y += cellSize) {
          positions.push(x, y, -half, x, y, half);
        }
      }

      const lineGeo = new THREE.BufferGeometry();
      lineGeo.setAttribute("position", new THREE.Float32BufferAttribute(positions, 3));
      const lineMat = new THREE.LineBasicMaterial({
        color: 0x9f9f9f,
        transparent: true,
        opacity: 0.2,
      });
      const baseLineGrid = new THREE.LineSegments(lineGeo, lineMat);
      this.gridHelper.add(baseLineGrid);

      this.gridCellCapacity = Math.max(1, Number(nRange?.max) || 0, particles.length);

      const boxGeo = new THREE.BoxGeometry(1, 1, 1);
      const fillMat = new THREE.MeshBasicMaterial({
        transparent: true,
        opacity: 0.16,
        depthWrite: false,
        vertexColors: true,
      });
      const highlightMat = new THREE.MeshBasicMaterial({
        transparent: true,
        opacity: 0.32,
        depthWrite: false,
        vertexColors: true,
      });

      this.gridCellFillMesh = new THREE.InstancedMesh(boxGeo, fillMat, this.gridCellCapacity);
      this.gridCellFillMesh.instanceMatrix.setUsage(THREE.DynamicDrawUsage);
      this.gridCellFillMesh.frustumCulled = false;

      this.gridCellHighlightMesh = new THREE.InstancedMesh(boxGeo.clone(), highlightMat, this.gridCellCapacity);
      this.gridCellHighlightMesh.instanceMatrix.setUsage(THREE.DynamicDrawUsage);
      this.gridCellHighlightMesh.frustumCulled = false;

      this.gridHelper.add(this.gridCellFillMesh);
      this.gridHelper.add(this.gridCellHighlightMesh);
      this.scene.add(this.gridHelper);
      this.updateGridVisibility();
    }

    updateGridVisibility() {
      if (!this.gridHelper) return;
      this.gridHelper.visible = this.config.mode === "grid";
      if (this.gridHelper.visible) this.updateGridCellsVisual();
    }

    updateGridCellsVisual() {
      if (!this.gridHelper || this.config.mode !== "grid") return;
      if (particles.length > this.gridCellCapacity) {
        this.rebuildGridHelper();
        if (!this.gridHelper || this.config.mode !== "grid") return;
      }

      const cellSize = Math.max(10, this.config.cellSize);
      const { info, buckets } = this.buildGridBuckets(cellSize);
      const maxOccupancy = Array.from(buckets.values()).reduce(
        (max, cell) => Math.max(max, cell.indices.length), 1
      );

      let selectedCellKey = null;
      let neighborKeys = null;
      if (selectedIndex >= 0 && selectedIndex < particles.length) {
        const p = particles[selectedIndex];
        const scx = info.toCellIndex(p.x, info.nx);
        const scy = info.toCellIndex(p.y, info.ny);
        const scz = info.toCellIndex(p.z, info.nz);
        selectedCellKey = info.key(scx, scy, scz);
        neighborKeys = new Set();
        for (let dz = -1; dz <= 1; dz++) {
          for (let dy = -1; dy <= 1; dy++) {
            for (let dx = -1; dx <= 1; dx++) {
              const cx = scx + dx, cy = scy + dy, cz = scz + dz;
              if (cx < 0 || cy < 0 || cz < 0 || cx >= info.nx || cy >= info.ny || cz >= info.nz) continue;
              neighborKeys.add(info.key(cx, cy, cz));
            }
          }
        }
      }

      let fillIndex = 0, highlightIndex = 0;
      for (const cell of buckets.values()) {
        const center = info.centerOf(cell.cx, cell.cy, cell.cz);
        this.tmpGridPosition.set(center.x, center.y, center.z);
        this.tmpGridScale.setScalar(cellSize * 0.94);
        this.tmpGridMatrix.compose(this.tmpGridPosition, this.identityQuat, this.tmpGridScale);
        this.gridCellFillMesh.setMatrixAt(fillIndex, this.tmpGridMatrix);

        const densityT = maxOccupancy <= 1 ? 0 : (cell.indices.length - 1) / (maxOccupancy - 1);
        this.tmpGridColor.copy(this.gridColors.gridSparseColor).lerp(this.gridColors.gridDenseColor, densityT);
        this.gridCellFillMesh.setColorAt(fillIndex, this.tmpGridColor);
        fillIndex++;

        if (neighborKeys && neighborKeys.has(cell.key)) {
          this.tmpGridScale.setScalar(cellSize * 1.02);
          this.tmpGridMatrix.compose(this.tmpGridPosition, this.identityQuat, this.tmpGridScale);
          this.gridCellHighlightMesh.setMatrixAt(highlightIndex, this.tmpGridMatrix);
          this.gridCellHighlightMesh.setColorAt(
            highlightIndex,
            cell.key === selectedCellKey ? this.gridColors.gridSelectedCellColor : this.gridColors.gridNeighborColor
          );
          highlightIndex++;
        }
      }

      this.gridCellFillMesh.count = fillIndex;
      this.gridCellFillMesh.instanceMatrix.needsUpdate = true;
      if (this.gridCellFillMesh.instanceColor) this.gridCellFillMesh.instanceColor.needsUpdate = true;

      this.gridCellHighlightMesh.count = highlightIndex;
      this.gridCellHighlightMesh.instanceMatrix.needsUpdate = true;
      if (this.gridCellHighlightMesh.instanceColor) this.gridCellHighlightMesh.instanceColor.needsUpdate = true;
    }

    rebuildSpheres() {
      for (const s of this.spheres) {
        this.scene.remove(s);
        s.geometry.dispose();
        if (s.userData.baseMat) s.userData.baseMat.dispose();
      }
      this.spheres.length = 0;
      for (let i = 0; i < particles.length; i++) {
        const p = particles[i];
        const geo = new THREE.SphereGeometry(p.r, 18, 18);
        const particleMat = this.baseMat.clone();
        particleMat.transparent = true;
        particleMat.opacity = 1;
        const mesh = new THREE.Mesh(geo, particleMat);
        mesh.userData.baseMat = particleMat;
        mesh.userData.index = i;
        mesh.position.set(p.x, p.y, p.z);
        this.spheres.push(mesh);
        this.scene.add(mesh);
      }
    }

    clearTrails() {
      for (const t of this.trailLines) {
        this.scene.remove(t.line);
        t.line.geometry.dispose();
        t.line.material.dispose();
      }
      this.trailLines.length = 0;
    }

    rebuildTrails() {
      this.clearTrails();
      const TRAIL_POINTS = 40;
      for (let i = 0; i < particles.length; i++) {
        const p = particles[i];
        const positions = new Float32Array(TRAIL_POINTS * 3);
        for (let k = 0; k < TRAIL_POINTS; k++) {
          const base = k * 3;
          positions[base] = p.x;
          positions[base + 1] = p.y;
          positions[base + 2] = p.z;
        }

        const geometry = new THREE.BufferGeometry();
        const attr = new THREE.BufferAttribute(positions, 3);
        geometry.setAttribute("position", attr);

        const material = new THREE.LineBasicMaterial({
          color: 0x6a6a6a,
          transparent: true,
          opacity: 0.35,
        });
        const line = new THREE.Line(geometry, material);
        line.frustumCulled = false;
        this.scene.add(line);

        this.trailLines.push({ line, positions, attr });
      }
    }

    updateTrails(advance = true) {
      const TRAIL_POINTS = 40;
      for (let i = 0; i < particles.length; i++) {
        const p = particles[i];
        const t = this.trailLines[i];
        if (!t) continue;

        if (advance) {
          t.positions.copyWithin(0, 3);
          const last = (TRAIL_POINTS - 1) * 3;
          t.positions[last] = p.x;
          t.positions[last + 1] = p.y;
          t.positions[last + 2] = p.z;
          t.attr.needsUpdate = true;
        }

        if (i === selectedIndex) {
          t.line.material.color.setHex(0xffb020);
          t.line.material.opacity = 0.7;
        } else {
          t.line.material.color.setHex(0x6a6a6a);
          t.line.material.opacity = 0.35;
        }
      }
    }

    updateSpheresTransform() {
      for (let i = 0; i < particles.length; i++) {
        const p = particles[i];
        const mesh = this.spheres[i];
        mesh.position.set(p.x, p.y, p.z);
        const currentR = mesh.geometry.parameters.radius;
        if (Math.abs(currentR - p.r) > 0.001) {
          mesh.geometry.dispose();
          mesh.geometry = new THREE.SphereGeometry(p.r, 18, 18);
        }

        if (i === selectedIndex) {
          mesh.material = this.selMat;
        } else {
          const mat = mesh.userData.baseMat;
          const base = new THREE.Color(p.color ?? 0x000000);
          const bright = Math.max(0.2, Number(p.bright ?? 1));
          base.multiplyScalar(bright);
          mat.color.copy(base);
          mat.opacity = 1;
          mesh.material = mat;
        }
      }
    }

    resize() {
      const w = this.container.clientWidth;
      const h = this.container.clientHeight;
      this.renderer.setSize(w, h, false);
      this.camera.aspect = w / h;
      this.camera.updateProjectionMatrix();
    }

    onPointerDown(e) {
      const rect = this.renderer.domElement.getBoundingClientRect();
      this.mouse.x = ((e.clientX - rect.left) / rect.width) * 2 - 1;
      this.mouse.y = -(((e.clientY - rect.top) / rect.height) * 2 - 1);

      this.raycaster.setFromCamera(this.mouse, this.camera);
      const hits = this.raycaster.intersectObjects(this.spheres, false);

      if (hits.length > 0) {
        const idx = hits[0].object.userData.index;
        selectedIndex = idx;
        if (isAdmin) {
          syncEditorFromParticle(particles[idx], idx);
        }
      } else {
        selectedIndex = -1;
        editorBox.style.display = "none";
      }
      renderPlanetList();
    }

    render() {
      this.controls.update();
      if (this.gridHelper && this.gridHelper.visible) this.updateGridCellsVisual();
      this.renderer.render(this.scene, this.camera);
    }
  }

  // ======= HELPER FUNCTIONS =======
  function isSolarScenario() {
    return Boolean(scenarioSel) && (
      scenarioSel.value === "solar" || scenarioSel.value === "solar_stable"
    );
  }

  function isStableSolarScenario() {
    return Boolean(scenarioSel) && scenarioSel.value === "solar_stable";
  }

  function getGravityConstant() {
    return isStableSolarScenario() ? 0.12 : G;
  }

  function getSofteningValue() {
    return isStableSolarScenario() ? 0.1 : softening;
  }

  function updateActiveViewControls() {
    if (!activeViewSel || !activeViewMode || !activeCellRange || !activeCellLabel) return;
    activeViewSel.value = String(activeViewIndex);
    const view = views[activeViewIndex];
    if (!view) return;
    activeViewMode.value = view.config.mode;
    activeCellRange.value = String(view.config.cellSize);
    activeCellLabel.textContent = String(view.config.cellSize);
  }

  function applyActiveViewConfig() {
    const view = views[activeViewIndex];
    if (!view) return;
    view.config.mode = activeViewMode.value || "naive";
    view.config.cellSize = Number(activeCellRange.value);
    if (view.gridHelper) {
      view.rebuildGridHelper();
    }
    view.updateGridVisibility();
    if (view.gridHelper) view.updateGridCellsVisual();
  }

  function volumeFromR(r) {
    return (4 / 3) * Math.PI * r * r * r;
  }

  function mulberry32(seed) {
    return function () {
      let t = (seed += 0x6d2b79f5);
      t = Math.imul(t ^ (t >>> 15), t | 1);
      t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
      return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };
  }

  function initParticles(N, seed = 12345) {
    const rand = mulberry32(seed);
    const box = 350;
    const uniformR = 4;
    const uniformRho = 1;
    const useRandomSizeMass = Boolean(randomSizeMass && randomSizeMass.checked);
    particles = [];
    for (let i = 0; i < N; i++) {
      const r = useRandomSizeMass ? 2 + rand() * 6 : uniformR;
      const rho = useRandomSizeMass ? 1 : uniformRho;
      const m = rho * volumeFromR(r);
      const hue = rand();
      const sat = 0.45 + rand() * 0.4;
      const light = 0.35 + rand() * 0.25;
      const color = new THREE.Color().setHSL(hue, sat, light).getHex();

      particles.push({
        x: (rand() - 0.5) * box, y: (rand() - 0.5) * box, z: (rand() - 0.5) * box,
        vx: (rand() - 0.5) * 0.8, vy: (rand() - 0.5) * 0.8, vz: (rand() - 0.5) * 0.8,
        ax: 0, ay: 0, az: 0, r, rho, m, color, bright: 1,
      });
    }
    selectedIndex = -1;
    editorBox.style.display = "none";
  }

  function initSolarSystemPreset() {
    const stable = isStableSolarScenario();
    const sunMass = stable ? 1000 : 120000;
    const sunRho = stable ? 2.2 : 1.5;
    const sunR = Math.cbrt((3 * sunMass) / (4 * Math.PI * sunRho));
    const grav = stable ? getGravityConstant() : G;

    const planetDefs = [
      stable ? { name: "Mercury", orbit: 3.9 * 10, mass: 0.055, radius: 1.2, color: 0xa9a9a9 } : { name: "Mercury", orbit: 45, mass: 20, radius: 3.2, color: 0xa9a9a9 },
      stable ? { name: "Venus", orbit: 7.2 * 10, mass: 0.815, radius: 1.8, color: 0xd9c27f } : { name: "Venus", orbit: 70, mass: 35, radius: 4.3, color: 0xd9c27f },
      stable ? { name: "Earth", orbit: 10 * 10, mass: 1.0, radius: 1.9, color: 0x4f83ff } : { name: "Earth", orbit: 95, mass: 38, radius: 4.5, color: 0x4f83ff },
      stable ? { name: "Mars", orbit: 15.2 * 10, mass: 0.107, radius: 1.4, color: 0xc86d4b } : { name: "Mars", orbit: 125, mass: 26, radius: 3.8, color: 0xc86d4b },
      stable ? { name: "Jupiter", orbit: 52 * 10, mass: 317.8, radius: 4.8, color: 0xd2b48c } : { name: "Jupiter", orbit: 175, mass: 420, radius: 10.5, color: 0xd2b48c },
      stable ? { name: "Saturn", orbit: 95.8 * 10, mass: 95.2, radius: 4.3, color: 0xe0cf8a } : { name: "Saturn", orbit: 235, mass: 300, radius: 9.2, color: 0xe0cf8a },
      stable ? { name: "Uranus", orbit: 192 * 10, mass: 14.5, radius: 3.1, color: 0x8fe7ff } : { name: "Uranus", orbit: 290, mass: 120, radius: 7.1, color: 0x8fe7ff },
      stable ? { name: "Neptune", orbit: 301 * 10, mass: 17.1, radius: 3.0, color: 0x4169e1 } : { name: "Neptune", orbit: 340, mass: 130, radius: 7.0, color: 0x4169e1 },
    ];

    particles = [{
      name: "Sun", x: 0, y: 0, z: 0, vx: 0, vy: 0, vz: 0, ax: 0, ay: 0, az: 0,
      r: sunR, rho: sunRho, m: sunMass, color: 0xfff2b3, bright: 1.8,
    }];

    let totalPlanetMomentumY = 0;
    for (const def of planetDefs) {
      const speed = stable
        ? Math.sqrt((grav * sunMass) / (60 * def.orbit))
        : Math.sqrt((grav * sunMass) / def.orbit) / 60;
      const rho = def.mass / volumeFromR(def.radius);
      particles.push({
        name: def.name, x: def.orbit, y: 0, z: 0, vx: 0, vy: speed, vz: 0,
        ax: 0, ay: 0, az: 0, r: def.radius, rho, m: def.mass, color: def.color, bright: 1.1,
      });
      totalPlanetMomentumY += def.mass * speed;
    }

    if (stable) {
      particles[0].vy = -totalPlanetMomentumY / sunMass;
    }

    selectedIndex = -1;
    editorBox.style.display = "none";
  }

  // ======= PHYSICS =======
  function computeAccelerationsNaive() {
    const grav = getGravityConstant();
    const soften = getSofteningValue();
    let ops = 0;
    for (const p of particles) p.ax = p.ay = p.az = 0;

    const N = particles.length;
    for (let i = 0; i < N; i++) {
      const a = particles[i];
      for (let j = i + 1; j < N; j++) {
        const b = particles[j];
        const dx = b.x - a.x;
        const dy = b.y - a.y;
        const dz = b.z - a.z;
        const dist2 = dx * dx + dy * dy + dz * dz + soften;
        const inv = 1 / Math.sqrt(dist2);
        const f = grav * inv * inv;
        const fx = f * dx * inv;
        const fy = f * dy * inv;
        const fz = f * dz * inv;
        a.ax += fx * b.m; a.ay += fy * b.m; a.az += fz * b.m;
        b.ax -= fx * a.m; b.ay -= fy * a.m; b.az -= fz * a.m;
        ops++;
      }
    }
    return ops;
  }

  function computeAccelerationsGrid(cellSize) {
    const grav = getGravityConstant();
    const soften = getSofteningValue();
    let ops = 0;
    for (const p of particles) p.ax = p.ay = p.az = 0;

    const view = views[0];
    const { info, buckets } = view.buildGridBuckets(cellSize);

    for (const cellA of buckets.values()) {
      const listA = cellA.indices;
      const idA = info.idOf(cellA.cx, cellA.cy, cellA.cz);

      for (let dz = -1; dz <= 1; dz++) {
        for (let dy = -1; dy <= 1; dy++) {
          for (let dx = -1; dx <= 1; dx++) {
            const nxCell = cellA.cx + dx;
            const nyCell = cellA.cy + dy;
            const nzCell = cellA.cz + dz;
            if (nxCell < 0 || nyCell < 0 || nzCell < 0 || nxCell >= info.nx || nyCell >= info.ny || nzCell >= info.nz) {
              continue;
            }

            const cellB = buckets.get(info.key(nxCell, nyCell, nzCell));
            if (!cellB) continue;
            const listB = cellB.indices;

            const idB = info.idOf(nxCell, nyCell, nzCell);
            if (idB < idA) continue;

            if (idB === idA) {
              for (let ia = 0; ia < listA.length; ia++) {
                const i = listA[ia];
                const a = particles[i];
                for (let jb = ia + 1; jb < listA.length; jb++) {
                  const j = listA[jb];
                  const b = particles[j];
                  const dx3 = b.x - a.x;
                  const dy3 = b.y - a.y;
                  const dz3 = b.z - a.z;
                  const dist2 = dx3 * dx3 + dy3 * dy3 + dz3 * dz3 + soften;
                  const inv = 1 / Math.sqrt(dist2);
                  const f = grav * inv * inv;
                  const fx = f * dx3 * inv;
                  const fy = f * dy3 * inv;
                  const fz = f * dz3 * inv;
                  a.ax += fx * b.m; a.ay += fy * b.m; a.az += fz * b.m;
                  b.ax -= fx * a.m; b.ay -= fy * a.m; b.az -= fz * a.m;
                  ops++;
                }
              }
            } else {
              for (let ia = 0; ia < listA.length; ia++) {
                const i = listA[ia];
                const a = particles[i];
                for (let jb = 0; jb < listB.length; jb++) {
                  const j = listB[jb];
                  const b = particles[j];
                  const dx3 = b.x - a.x;
                  const dy3 = b.y - a.y;
                  const dz3 = b.z - a.z;
                  const dist2 = dx3 * dx3 + dy3 * dy3 + dz3 * dz3 + soften;
                  const inv = 1 / Math.sqrt(dist2);
                  const f = grav * inv * inv;
                  const fx = f * dx3 * inv;
                  const fy = f * dy3 * inv;
                  const fz = f * dz3 * inv;
                  a.ax += fx * b.m; a.ay += fy * b.m; a.az += fz * b.m;
                  b.ax -= fx * a.m; b.ay -= fy * a.m; b.az -= fz * a.m;
                  ops++;
                }
              }
            }
          }
        }
      }
    }
    return ops;
  }

  function leapfrogKickDrift() {
    const dtScaled = dt * timeScale;
    const half = dtScaled * 0.5;
    const useBounds = !isStableSolarScenario();
    for (const p of particles) {
      p.vx += p.ax * half; p.vy += p.ay * half; p.vz += p.az * half;
      p.x += p.vx * 60 * dtScaled;
      p.y += p.vy * 60 * dtScaled;
      p.z += p.vz * 60 * dtScaled;

      const r = p.r || 2;
      if (useBounds) {
        if (p.x < -BOX_LIMIT + r) { p.x = -BOX_LIMIT + r; p.vx *= -1; }
        if (p.x > BOX_LIMIT - r) { p.x = BOX_LIMIT - r; p.vx *= -1; }
        if (p.y < -BOX_LIMIT + r) { p.y = -BOX_LIMIT + r; p.vy *= -1; }
        if (p.y > BOX_LIMIT - r) { p.y = BOX_LIMIT - r; p.vy *= -1; }
        if (p.z < -BOX_LIMIT + r) { p.z = -BOX_LIMIT + r; p.vz *= -1; }
        if (p.z > BOX_LIMIT - r) { p.z = BOX_LIMIT - r; p.vz *= -1; }
      }
    }
  }

  function leapfrogKick() {
    const dtScaled = dt * timeScale;
    const half = dtScaled * 0.5;
    const lossFactor = Math.max(0, 1 - getEffectiveEnergyLoss() * dtScaled * 60);
    const dampingFactor = getDampingFactor();
    for (const p of particles) {
      p.vx = (p.vx + p.ax * half) * dampingFactor * lossFactor;
      p.vy = (p.vy + p.ay * half) * dampingFactor * lossFactor;
      p.vz = (p.vz + p.az * half) * dampingFactor * lossFactor;
    }
  }

  function getDampingFactor() {
    return isStableSolarScenario() ? 1 : damping;
  }

  function getEffectiveEnergyLoss() {
    return isStableSolarScenario() ? 0 : energyLoss;
  }

  function getSafeTimeScale(cellSize, desiredTimeScale) {
    if (particles.length === 0) return desiredTimeScale;

    let maxSpeed = 0;
    let minRadius = Infinity;
    for (const p of particles) {
      const speed = Math.hypot(p.vx || 0, p.vy || 0, p.vz || 0);
      if (speed > maxSpeed) maxSpeed = speed;
      const radius = Math.max(0.1, p.r || 0.1);
      if (radius < minRadius) minRadius = radius;
    }
    if (maxSpeed <= 0) return desiredTimeScale;

    const maxMoveByRadius = minRadius * 0.4;
    const maxMoveByCell = Math.max(1, cellSize * 0.5);
    const safeDistance = Math.min(maxMoveByRadius, maxMoveByCell);

    const safeTimeScale = safeDistance / (maxSpeed * dt);
    return Math.max(0.01, Math.min(desiredTimeScale, safeTimeScale));
  }

  function updateSpeedLimitNotice(desiredTimeScale, appliedTimeScale) {
    if (!speedLimitNotice) return;
    if (appliedTimeScale < desiredTimeScale) {
      speedLimitNotice.textContent = `Speed auto-limited to ${appliedTimeScale.toFixed(2)}x to avoid tunneling / missed collisions`;
    } else {
      speedLimitNotice.textContent = "\u00A0";
    }
  }

  function handleCollisionsAndMerge() {
    if (collisionMode === "bounce") {
      return handleCollisionsAndBounce();
    }
    if (isStableSolarScenario()) return false;
    if (particles.length < 2) return false;
    let merged = false;

    for (let i = 0; i < particles.length; i++) {
      const a = particles[i];
      for (let j = i + 1; j < particles.length; j++) {
        const b = particles[j];
        const dx = b.x - a.x;
        const dy = b.y - a.y;
        const dz = b.z - a.z;
        const dist2 = dx * dx + dy * dy + dz * dz;
        const minDist = (a.r || 0) + (b.r || 0);

        if (dist2 <= minDist * minDist) {
          const mA = a.m;
          const mB = b.m;
          const mT = mA + mB;
          if (mT <= 0) continue;

          const vx = (a.vx * mA + b.vx * mB) / mT;
          const vy = (a.vy * mA + b.vy * mB) / mT;
          const vz = (a.vz * mA + b.vz * mB) / mT;

          const x = (a.x * mA + b.x * mB) / mT;
          const y = (a.y * mA + b.y * mB) / mT;
          const z = (a.z * mA + b.z * mB) / mT;

          const rhoA = a.rho ?? 1;
          const rhoB = b.rho ?? 1;
          const rho = (rhoA * mA + rhoB * mB) / mT;
          const r = Math.cbrt((3 * mT) / (4 * Math.PI * rho));

          const color = (mA > mB) ? a.color : b.color;
          const bright = (mA > mB) ? (a.bright ?? 1) : (b.bright ?? 1);

          const keep = i;
          const drop = j;

          particles[keep] = {
            x, y, z, vx, vy, vz, ax: 0, ay: 0, az: 0, r, rho, m: mT, color, bright,
          };
          particles.splice(drop, 1);

          if (selectedIndex === drop) selectedIndex = keep;
          if (selectedIndex > drop) selectedIndex -= 1;

          merged = true;
          j -= 1;
        }
      }
    }

    return merged;
  }

  function handleCollisionsAndBounce() {
    if (isStableSolarScenario()) return false;
    if (particles.length < 2) return false;
    let bounced = false;

    for (let i = 0; i < particles.length; i++) {
      const a = particles[i];
      for (let j = i + 1; j < particles.length; j++) {
        const b = particles[j];
        const dx = b.x - a.x;
        const dy = b.y - a.y;
        const dz = b.z - a.z;
        const dist2 = dx * dx + dy * dy + dz * dz;
        const minDist = (a.r || 0) + (b.r || 0);

        if (dist2 <= minDist * minDist && dist2 > 0) {
          const dist = Math.sqrt(dist2);
          const nx = dx / dist;
          const ny = dy / dist;
          const nz = dz / dist;
          const overlap = minDist - dist;
          const invMassA = a.m > 0 ? 1 / a.m : 0;
          const invMassB = b.m > 0 ? 1 / b.m : 0;
          const invMassTotal = invMassA + invMassB;

          if (invMassTotal > 0) {
            const pushA = overlap * (invMassA / invMassTotal);
            const pushB = overlap * (invMassB / invMassTotal);
            a.x -= nx * pushA;
            a.y -= ny * pushA;
            a.z -= nz * pushA;
            b.x += nx * pushB;
            b.y += ny * pushB;
            b.z += nz * pushB;
          }

          const rvx = b.vx - a.vx;
          const rvy = b.vy - a.vy;
          const rvz = b.vz - a.vz;
          const relVel = rvx * nx + rvy * ny + rvz * nz;
          if (relVel >= 0) {
            bounced = true;
            continue;
          }

          const restitution = 1.0;
          const impulseMagnitude = -(1 + restitution) * relVel / invMassTotal;
          const impulseX = impulseMagnitude * nx;
          const impulseY = impulseMagnitude * ny;
          const impulseZ = impulseMagnitude * nz;

          a.vx -= impulseX * invMassA;
          a.vy -= impulseY * invMassA;
          a.vz -= impulseZ * invMassA;
          b.vx += impulseX * invMassB;
          b.vy += impulseY * invMassB;
          b.vz += impulseZ * invMassB;
          bounced = true;
        }
      }
    }

    return bounced;
  }

  async function stepBackend3D(cellSize) {
    const payload = {
      dimension: "3d",
      particles,
      mode: modeSel.value,
      collisionMode,
      cellSize,
      timeScale,
      energyLoss,
      stable: isStableSolarScenario(),
      dt,
      G,
      softening,
    };
    if (collisionMode === "bounce") {
      let ops = 0;
      if (modeSel.value === "naive") {
        ops = computeAccelerationsNaive();
      } else {
        ops = computeAccelerationsGrid(cellSize);
      }
      leapfrogKickDrift();
      if (modeSel.value === "naive") {
        ops += computeAccelerationsNaive();
      } else {
        ops += computeAccelerationsGrid(cellSize);
      }
      leapfrogKick();
      const merged = handleCollisionsAndMerge();
      return { ops, merged };
    }

    try {
      const result = await window.stepSimulation(payload);
      if (Array.isArray(result.particles)) {
        particles = result.particles;
      }
      return { ops: Number(result.ops) || 0, merged: Boolean(result.merged) };
    } catch (error) {
      console.error("3D backend step failed, falling back to local compute:", error);
      let ops = 0;
      if (modeSel.value === "naive") {
        ops = computeAccelerationsNaive();
      } else {
        ops = computeAccelerationsGrid(cellSize);
      }
      leapfrogKickDrift();
      if (modeSel.value === "naive") {
        ops += computeAccelerationsNaive();
      } else {
        ops += computeAccelerationsGrid(cellSize);
      }
      leapfrogKick();
      const merged = handleCollisionsAndMerge();
      return { ops, merged };
    }
  }

  function computeTotalEnergy() {
    const grav = getGravityConstant();
    const soften = getSofteningValue();
    let kinetic = 0;
    for (const p of particles) {
      kinetic += 0.5 * p.m * (p.vx * p.vx + p.vy * p.vy + p.vz * p.vz);
    }

    let potential = 0;
    for (let i = 0; i < particles.length; i++) {
      const a = particles[i];
      for (let j = i + 1; j < particles.length; j++) {
        const b = particles[j];
        const dx = b.x - a.x;
        const dy = b.y - a.y;
        const dz = b.z - a.z;
        const dist = Math.sqrt(dx * dx + dy * dy + dz * dz + soften);
        potential += -grav * a.m * b.m / dist;
      }
    }

    return kinetic + potential;
  }

  // ======= UI & EDITOR =======
  function syncEditorFromParticle(p, idx) {
    editorBox.style.display = "block";
    selName.textContent = `#${idx}`;

    rInput.value = p.r.toFixed(1);
    rhoInput.value = (p.rho ?? 1).toFixed(1);
    mInput.value = p.m.toFixed(2);

    rVal.textContent = Number(rInput.value).toFixed(1);
    rhoVal.textContent = Number(rhoInput.value).toFixed(1);
    mVal.textContent = Number(mInput.value).toFixed(2);

    if (colorInput) colorInput.value = `#${(p.color ?? 0).toString(16).padStart(6, "0")}`;
    if (brightInput) brightInput.value = String(p.bright ?? 1);
    if (brightVal) brightVal.textContent = Number(brightInput.value).toFixed(1);
  }

  function applyRhoAndRToMass(p) {
    const r = Math.max(0.1, Number(rInput.value));
    const rho = Math.max(0.0001, Number(rhoInput.value));

    p.r = r;
    p.rho = rho;
    p.m = rho * volumeFromR(r);

    rVal.textContent = r.toFixed(1);
    rhoVal.textContent = rho.toFixed(1);
    mInput.value = p.m.toFixed(2);
    mVal.textContent = p.m.toFixed(2);
  }

  function applyMassToRho(p) {
    const r = Math.max(0.1, Number(rInput.value));
    const m = Math.max(0.0001, Number(mInput.value));

    p.r = r;
    p.m = m;

    const rho = m / volumeFromR(r);
    p.rho = rho;

    rVal.textContent = r.toFixed(1);
    mVal.textContent = m.toFixed(2);
    rhoInput.value = rho.toFixed(1);
    rhoVal.textContent = Number(rhoInput.value).toFixed(1);
  }

  function formatPlanetValue(v) {
    return Number(v).toFixed(1);
  }

  function renderPlanetList() {
    if (!planetListEl || !planetCountEl) return;

    planetCountEl.textContent = `${particles.length} planets`;
    if (particles.length === 0) {
      planetListEl.innerHTML = '<div class="planetRow"><span class="planetMeta">No planets</span></div>';
      return;
    }

    planetListEl.innerHTML = particles
      .map((p, i) => {
        const activeClass = i === selectedIndex ? " active" : "";
        const title = p.name ? `${p.name} ` : "";
        return `<div class="planetRow${activeClass}">
          <button type="button" class="planetSelectBtn" data-planet-index="${i}">#${i}</button>
          <span class="planetMeta">${title}r ${formatPlanetValue(p.r)} | m ${formatPlanetValue(p.m)}</span>
          <span class="planetMeta">rho ${formatPlanetValue(p.rho ?? 1)}</span>
        </div>`;
      })
      .join("");
  }

  function updateAdminUIVisibility() {
    adminElements.forEach(id => {
      const el = document.getElementById(id);
      if (el) {
        el.style.display = isAdmin ? "" : "none";
        el.disabled = !isAdmin;
      }
    });
  }

  function handleLogin() {
    const password = adminPassword.value;
    if (password === ADMIN_PASSWORD) {
      isAdmin = true;
      adminPassword.value = "";
      modalLogin.style.display = "none";

      loginBtn.style.display = "none";
      logoutBtn.style.display = "block";
      loginStatus.textContent = "✓ 已登入為管理員";
      loginStatus.style.color = "#4CAF50";

      updateAdminUIVisibility();
    } else {
      alert("密碼錯誤！");
      adminPassword.value = "";
    }
  }

  function handleLogout() {
    isAdmin = false;
    modalLogin.style.display = "none";

    loginBtn.style.display = "block";
    logoutBtn.style.display = "none";
    loginStatus.textContent = "未登入";
    loginStatus.style.color = "#666";

    editorBox.style.display = "none";

    updateAdminUIVisibility();
  }

  function addPlanetFromInputs() {
    if (!addX || !addY || !addZ || !addVx || !addVy || !addVz || !addR || !addRho) return;
    const x = Number(addX.value);
    const y = Number(addY.value);
    const z = Number(addZ.value);
    const vx = Number(addVx.value);
    const vy = Number(addVy.value);
    const vz = Number(addVz.value);
    const r = Math.max(0.1, Number(addR.value));
    const rho = Math.max(0.0001, Number(addRho.value));
    const m = rho * volumeFromR(r);
    const color = addColor ? parseInt(addColor.value.replace("#", ""), 16) : 0x000000;
    const bright = addBright ? Math.max(0.2, Number(addBright.value)) : 1;

    particles.push({
      x: Number.isFinite(x) ? x : 0,
      y: Number.isFinite(y) ? y : 0,
      z: Number.isFinite(z) ? z : 0,
      vx: Number.isFinite(vx) ? vx : 0,
      vy: Number.isFinite(vy) ? vy : 0,
      vz: Number.isFinite(vz) ? vz : 0,
      ax: 0, ay: 0, az: 0,
      r, rho, m, color, bright,
    });

    selectedIndex = particles.length - 1;
    syncEditorFromParticle(particles[selectedIndex], selectedIndex);

    if (nRange) {
      if (particles.length > Number(nRange.max)) {
        nRange.max = String(particles.length);
      }
      nRange.value = String(particles.length);
      nLabel.textContent = String(particles.length);
    }

    views.forEach(v => {
      v.rebuildSpheres();
      v.rebuildTrails();
    });
    renderPlanetList();
  }

  function updateScenarioUI() {
    const isSolar = isSolarScenario();
    nRange.disabled = Boolean(isSolar);
    nLabel.textContent = isSolar ? "9" : nRange.value;
    lossRange.disabled = isStableSolarScenario();
    if (isStableSolarScenario()) lossRange.value = "0";
  }

  function updateSpeedLabel() {
    speedLabel.textContent = `${Number(speedRange.value).toFixed(2)}x`;
  }

  function updateLossLabel() {
    const lossValue = isStableSolarScenario() ? 0 : Number(lossRange.value);
    lossLabel.textContent = lossValue.toFixed(3);
  }

  function resetAll() {
    cellLabel.textContent = cellRange.value;
    updateScenarioUI();

    if (isSolarScenario()) {
      initSolarSystemPreset();
    } else {
      const N = +nRange.value;
      initParticles(N, 12345);
    }

    views.forEach(v => {
      v.rebuildSpheres();
      v.rebuildTrails();
      v.rebuildGridHelper();
    });
    renderPlanetList();

    views.forEach(v => {
      v.lastNaiveOpsBaseline = null;
      v.lastNForBaseline = null;
    });
  }

  // ======= MAIN LOOP =======
  async function loop() {
    const N = particles.length;
    const t0 = performance.now();
    let ops = 0;
    let merged = false;

    const desiredTimeScale = Math.max(0.01, +speedRange.value || 1);
    energyLoss = Math.max(0, +lossRange.value || 0);
    const cellSize = Number(cellRange.value);

    timeScale = getSafeTimeScale(cellSize, desiredTimeScale);
    updateSpeedLimitNotice(desiredTimeScale, timeScale);

    if (!paused) {
      const result = await stepBackend3D(cellSize);
      ops = result.ops;
      merged = result.merged;
      if (merged) {
        views.forEach(v => {
          v.rebuildSpheres();
          v.rebuildTrails();
        });
        renderPlanetList();
      }
    }

    const t1 = performance.now();
    const ms = t1 - t0;

    views.forEach(v => {
      v.updateTrails(!paused);
      v.updateSpheresTransform();
      v.render();
    });

    msEl.textContent = paused ? "paused" : ms.toFixed(2);
    opsEl.textContent = paused ? "-" : ops.toString();
    eiEl.textContent = paused ? "-" : "100%";
    tcEl.textContent = paused ? "-" : "O(N²)";
    if (!paused) {
      energyFrameCounter = (energyFrameCounter + 1) % 10;
      if (energyFrameCounter === 0) {
        const totalEnergy = computeTotalEnergy();
        energyEl.textContent = totalEnergy.toFixed(2);
      }
    }

    requestAnimationFrame(loop);
  }

  function updatePanelVisibility() {
  const viewBoxes = document.querySelectorAll(".viewBox");
  const viewGrid = document.querySelector(".viewGrid");

  let visibleCount = 0;

  viewBoxes.forEach((box, i) => {
    if (panelVisibility[i]) {
      box.style.display = "block";
      visibleCount++;
    } else {
      box.style.display = "none";
    }
  });

  if (!viewGrid) return;

  if (visibleCount === 1) {
    viewGrid.style.gridTemplateColumns = "1fr";
    viewGrid.style.gridTemplateRows = "1fr";
  } else if (visibleCount === 2) {
    viewGrid.style.gridTemplateColumns = "1fr 1fr";
    viewGrid.style.gridTemplateRows = "1fr";
  } else if (visibleCount === 3) {
    viewGrid.style.gridTemplateColumns = "1fr 1fr";
    viewGrid.style.gridTemplateRows = "1fr 1fr";
  } else {
    viewGrid.style.gridTemplateColumns = "1fr 1fr";
    viewGrid.style.gridTemplateRows = "1fr 1fr";
  }

  requestAnimationFrame(() => {
    views.forEach(v => v.resize());
  });
  }
  // ======= INITIALIZATION =======
  function initializeUI() {

    // 设置panel容器布局
    panelContainer.style.display = "flex";
    panelContainer.style.flexDirection = "column";
    panelContainer.style.gap = "0";

    // checkbox 事件
    document.querySelectorAll(".panelCheck").forEach((cb) => {
      const index = Number(cb.dataset.panel);
      if (!Number.isFinite(index) || index < 0 || index >= panelVisibility.length) return;
      panelVisibility[index] = cb.checked;
      cb.addEventListener("change", () => {
        panelVisibility[index] = cb.checked;
        updatePanelVisibility();
      });
    });

    // 登入事件
    if (loginBtn) loginBtn.addEventListener("click", () => { modalLogin.style.display = "flex"; adminPassword.focus(); });
    if (logoutBtn) logoutBtn.addEventListener("click", handleLogout);
    if (submitLogin) submitLogin.addEventListener("click", handleLogin);
    if (adminPassword) adminPassword.addEventListener("keypress", (e) => { if (e.key === "Enter") handleLogin(); });
    if (cancelLogin) cancelLogin.addEventListener("click", () => { modalLogin.style.display = "none"; adminPassword.value = ""; });
    if (closeLoginModal) closeLoginModal.addEventListener("click", () => { modalLogin.style.display = "none"; adminPassword.value = ""; });

    updateAdminUIVisibility();

    // 创建4个view
    for (const config of VIEW_CONFIGS) {
      views.push(new View3D(config));
    }

    // 事件监听
    nRange.addEventListener("input", () => { if (!isSolarScenario()) nLabel.textContent = nRange.value; });
    cellRange.addEventListener("input", () => { cellLabel.textContent = cellRange.value; views.forEach(v => v.rebuildGridHelper()); });
    if (activeViewSel) {
      activeViewSel.addEventListener("change", () => {
        activeViewIndex = Number(activeViewSel.value);
        updateActiveViewControls();
      });
    }
    if (activeViewMode) {
      activeViewMode.addEventListener("change", applyActiveViewConfig);
    }
    if (activeCellRange) {
      activeCellRange.addEventListener("input", () => {
        activeCellLabel.textContent = String(activeCellRange.value);
        applyActiveViewConfig();
      });
    }
    speedRange.addEventListener("input", updateSpeedLabel);
    lossRange.addEventListener("input", updateLossLabel);
    if (scenarioSel) scenarioSel.addEventListener("change", resetAll);
    if (collisionModeSel) {
      collisionModeSel.value = collisionMode;
      collisionModeSel.addEventListener("change", () => {
        collisionMode = collisionModeSel.value;
      });
    }
    resetBtn.addEventListener("click", resetAll);
    if (addPlanetBtn) addPlanetBtn.addEventListener("click", addPlanetFromInputs);

    async function loadBackendConfig3D() {
      try {
        const config = await window.fetchConfig();
        if (config?.default3D) {
          modeSel.value = config.default3D.mode || modeSel.value;
          nRange.value = config.default3D.n || nRange.value;
          cellRange.value = config.default3D.cellSize || cellRange.value;
          speedRange.value = config.default3D.speed || speedRange.value;
          lossRange.value = config.default3D.loss || lossRange.value;
          nLabel.textContent = nRange.value;
          cellLabel.textContent = cellRange.value;
          speedLabel.textContent = `${Number(speedRange.value).toFixed(2)}x`;
          lossLabel.textContent = Number(lossRange.value).toFixed(3);
        }
      } catch (error) {
        console.warn("Failed to load backend config:", error);
      }
    }

    loadBackendConfig3D();
    if (planetListToggleBtn) planetListToggleBtn.addEventListener("click", () => {
      const isHidden = planetListBody.style.display === "none";
      planetListBody.style.display = isHidden ? "block" : "none";
      planetListToggleBtn.textContent = isHidden ? "Hide" : "Show";
    });
    if (planetListEl) {
      planetListEl.addEventListener("click", (e) => {
        const btn = e.target.closest("[data-planet-index]");
        if (!btn) return;
        const idx = Number(btn.dataset.planetIndex);
        if (!Number.isFinite(idx) || idx < 0 || idx >= particles.length) return;
        selectedIndex = idx;
        if (isAdmin) syncEditorFromParticle(particles[idx], idx);
        renderPlanetList();
      });
    }
    pauseBtn.addEventListener("click", () => {
      paused = !paused;
      pauseBtn.textContent = paused ? "Resume" : "Pause";
    });

    rInput.addEventListener("input", () => {
      if (selectedIndex < 0) return;
      applyRhoAndRToMass(particles[selectedIndex]);
      renderPlanetList();
    });
    rhoInput.addEventListener("input", () => {
      if (selectedIndex < 0) return;
      applyRhoAndRToMass(particles[selectedIndex]);
      renderPlanetList();
    });
    mInput.addEventListener("input", () => {
      if (selectedIndex < 0) return;
      applyMassToRho(particles[selectedIndex]);
      renderPlanetList();
    });
    if (colorInput) {
      colorInput.addEventListener("input", () => {
        if (selectedIndex < 0) return;
        particles[selectedIndex].color = parseInt(colorInput.value.replace("#", ""), 16);
        renderPlanetList();
      });
    }
    if (brightInput) {
      brightInput.addEventListener("input", () => {
        if (selectedIndex < 0) return;
        const v = Math.max(0.2, Number(brightInput.value));
        particles[selectedIndex].bright = v;
        if (brightVal) brightVal.textContent = v.toFixed(1);
        renderPlanetList();
      });
    }


    const collapseSidebarBtn = document.getElementById("collapseSidebarBtn");
    const wrap = document.querySelector(".wrap");
    const panelToggleBtn = document.getElementById("togglePanelToggleBtn");
    const panelToggle = document.getElementById("panelToggle");

    if (panelToggleBtn && panelToggle) {
      panelToggleBtn.addEventListener("click", () => {
        const collapsed = panelToggle.classList.toggle("collapsed");
        panelToggleBtn.textContent = collapsed ? "展開" : "收起";

        // 內容寬度改變後需讓 3D 視圖重設尺寸，避免畫面空白或失真
        requestAnimationFrame(() => {
          views.forEach(v => v.resize());
        });
      });
    }

    if (collapseSidebarBtn && wrap) {
      collapseSidebarBtn.addEventListener("click", () => {
        wrap.classList.toggle("sidebarCollapsed");

        const collapsed = wrap.classList.contains("sidebarCollapsed");
        collapseSidebarBtn.textContent = collapsed ? "展開面板" : "收起面板";

        requestAnimationFrame(() => {
          views.forEach(v => v.resize());
        });
      });
    }
    updateSpeedLabel();
    updateLossLabel();
    updateScenarioUI();
    updatePanelVisibility();
    updateActiveViewControls();
    resetAll();
    loop();
  }

  initializeUI();
})();
