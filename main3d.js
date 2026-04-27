
import * as THREE from "https://cdn.jsdelivr.net/npm/three@0.160.0/build/three.module.js";//不是每個人都要先安裝 Three.js。 只要他打開網頁時有網路，瀏覽器就會去 jsDelivr 下載 Three.js。
import { OrbitControls } from "https://cdn.jsdelivr.net/npm/three@0.160.0/examples/jsm/controls/OrbitControls.js";
/*
Three.js 可以幫你做：
場景 scene
相機 camera
渲染器 renderer
球體 sphere
光源 light
滑鼠旋轉視角 OrbitControls
*/

(() => {
  // ======= UI Element References =======
  // Scenario selection dropdown
  const scenarioSel = document.getElementById("scenario");
  // Simulation mode selector (naive/grid)
  const modeSel = document.getElementById("mode");
  // Particle count slider
  const nRange = document.getElementById("nRange");
  // Particle count label
  const nLabel = document.getElementById("nLabel");
  // Grid cell size slider
  const cellRange = document.getElementById("cellRange");
  // Grid cell size label
  const cellLabel = document.getElementById("cellLabel");
  // Simulation speed slider
  const speedRange = document.getElementById("speedRange");
  // Simulation speed label
  const speedLabel = document.getElementById("speedLabel");
  // Energy loss slider
  const lossRange = document.getElementById("lossRange");
  // Energy loss label
  const lossLabel = document.getElementById("lossLabel");
  // Add planet button
  const addPlanetBtn = document.getElementById("addPlanetBtn");
  // Input fields for new planet position
  const addX = document.getElementById("addX");
  const addY = document.getElementById("addY");
  const addZ = document.getElementById("addZ");
  // Input fields for new planet velocity
  const addVx = document.getElementById("addVx");
  const addVy = document.getElementById("addVy");
  const addVz = document.getElementById("addVz");
  // Input fields for new planet radius and density
  const addR = document.getElementById("addR");
  const addRho = document.getElementById("addRho");
  // Reset simulation button
  const resetBtn = document.getElementById("resetBtn");
  // Pause/resume button
  const pauseBtn = document.getElementById("pauseBtn");

  // ======= Performance Display Elements =======
  // Milliseconds per frame display
  const msEl = document.getElementById("ms");
  // Operations per frame display
  const opsEl = document.getElementById("ops");
  // Efficiency index display
  const eiEl = document.getElementById("ei");
  // Time complexity display
  const tcEl = document.getElementById("tc");
  // Total energy display
  const energyEl = document.getElementById("energy");
  // Planet editor panel
  const editorBox = document.getElementById("editorBox");
  // Selected planet name display
  const selName = document.getElementById("selName");
  // Planet property input fields
  const rInput = document.getElementById("rInput");
  const rhoInput = document.getElementById("rhoInput");
  const mInput = document.getElementById("mInput");
  // Planet property value displays
  const rVal = document.getElementById("rVal");
  const rhoVal = document.getElementById("rhoVal");
  const mVal = document.getElementById("mVal");
  // Color and brightness inputs
  const colorInput = document.getElementById("colorInput");
  const brightInput = document.getElementById("brightInput");
  const brightVal = document.getElementById("brightVal");
  // Planet list toggle button and panel
  const planetListToggleBtn = document.getElementById("planetListToggleBtn");
  const planetListBody = document.getElementById("planetListBody");
  // Planet count display
  const planetCountEl = document.getElementById("planetCount");
  // Planet list container
  const planetListEl = document.getElementById("planetList");
  // Additional inputs for new planet
  const addColor = document.getElementById("addColor");
  const addBright = document.getElementById("addBright");

  // ======= Login UI Elements =======
  const loginBtn = document.getElementById("loginBtn");
  const logoutBtn = document.getElementById("logoutBtn");
  const loginStatus = document.getElementById("loginStatus");
  const modalLogin = document.getElementById("modalLogin");
  const adminPassword = document.getElementById("adminPassword");
  const submitLogin = document.getElementById("submitLogin");
  const cancelLogin = document.getElementById("cancelLogin");
  const closeLoginModal = document.getElementById("closeLoginModal");

  // ======= Authorization & Authentication =======
  let isAdmin = false;
  const ADMIN_PASSWORD = "123"; 
  
  // 管理員專用功能的元素ID
  const adminElements = [
    "scenario", // Scenario selection
    "nRange", "nLabel", // Particle count
    "cellRange", "cellLabel", // Cell size
    "lossRange", "lossLabel", // Energy loss
    "resetBtn", // Reset button
    "addPlanetBtn", "addX", "addY", "addZ", "addVx", "addVy", "addVz", "addR", "addRho", "addColor", "addBright", // Add planet
    "rInput", "rhoInput", "mInput", "colorInput", "brightInput", // Planet editor
    "editorBox" // Planet editor panel
  ];
/*{
  x: 位置X,
  y: 位置Y,
  z: 位置Z,

  vx: 速度X,
  vy: 速度Y,
  vz: 速度Z,

  ax: 加速度X,
  ay: 加速度Y,
  az: 加速度Z,

  r: 半徑,
  rho: 密度,
  m: 質量,
  color: 顏色,
  bright: 亮度
} */
  // ======= Three.js setup =======
  const view = document.getElementById("view3d");

  // ======= Authorization Functions =======
  function updateAdminUIVisibility() {
    // 根據管理員狀態更新UI可見性
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
      
      // 更新登入/登出按鈕
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
    
    // 更新登入/登出按鈕
    loginBtn.style.display = "block";
    logoutBtn.style.display = "none";
    loginStatus.textContent = "未登入";
    loginStatus.style.color = "#666";
    
    // 隱藏編輯框
    editorBox.style.display = "none";
    
    updateAdminUIVisibility();
  }

  // 登入事件監聽
  if (loginBtn) {
    loginBtn.addEventListener("click", () => {
      modalLogin.style.display = "flex";
      adminPassword.focus();
    });
  }

  if (logoutBtn) {
    logoutBtn.addEventListener("click", handleLogout);
  }

  if (submitLogin) {
    submitLogin.addEventListener("click", handleLogin);
  }

  if (adminPassword) {
    adminPassword.addEventListener("keypress", (e) => {
      if (e.key === "Enter") handleLogin();
    });
  }

  if (cancelLogin) {
    cancelLogin.addEventListener("click", () => {
      modalLogin.style.display = "none";
      adminPassword.value = "";
    });
  }

  if (closeLoginModal) {
    closeLoginModal.addEventListener("click", () => {
      modalLogin.style.display = "none";
      adminPassword.value = "";
    });
  }

  // 初始化 - 隱藏所有管理員功能
  updateAdminUIVisibility();

  const scene = new THREE.Scene();
  scene.background = new THREE.Color(0xe6e6e6);
  const BOX_LIMIT = 380;
  scene.fog = null;

  const camera = new THREE.PerspectiveCamera(60, 1, 0.1, 5000);
  camera.position.set(0, 200, 400);

  const renderer = new THREE.WebGLRenderer({ antialias: true });
  renderer.setPixelRatio(window.devicePixelRatio || 1);
  view.appendChild(renderer.domElement);

  const controls = new OrbitControls(camera, renderer.domElement);
  controls.enableDamping = true;
  controls.enablePan = true;
  controls.screenSpacePanning = true;
  controls.minDistance = 30;
  controls.maxDistance = 1400;
  scene.add(new THREE.AmbientLight(0xffffff, 0.7));
  const dir = new THREE.DirectionalLight(0xffffff, 0.8);
  dir.position.set(200, 400, 300);
  scene.add(dir);
  let gridHelper = null;
  let gridCellFillMesh = null;
  let gridCellHighlightMesh = null;
  let gridCellCapacity = 0;
  const gridSparseColor = new THREE.Color(0xc7d2df);
  const gridDenseColor = new THREE.Color(0x34587c);
  const gridNeighborColor = new THREE.Color(0xffc04d);
  const gridSelectedCellColor = new THREE.Color(0xff7a00);
  const tmpGridColor = new THREE.Color();
  const tmpGridScale = new THREE.Vector3();
  const tmpGridPosition = new THREE.Vector3();
  const tmpGridMatrix = new THREE.Matrix4();
  const identityQuat = new THREE.Quaternion();

  function getGridInfo(cellSize) {
    const span = BOX_LIMIT * 2;
    const nx = Math.max(1, Math.floor(span / cellSize));
    const ny = Math.max(1, Math.floor(span / cellSize));
    const nz = Math.max(1, Math.floor(span / cellSize));

    return {
      cellSize,
      nx,
      ny,
      nz,
      key(cx, cy, cz) {
        return `${cx},${cy},${cz}`;
      },
      idOf(cx, cy, cz) {
        return cx + cy * nx + cz * nx * ny;
      },
      toCellIndex(v, n) {//把位置轉換成格子索引
        const i = Math.floor((v + BOX_LIMIT) / cellSize);
        return Math.min(n - 1, Math.max(0, i));
      },
      centerOf(cx, cy, cz) {
        return {
          x: -BOX_LIMIT + (cx + 0.5) * cellSize,
          y: -BOX_LIMIT + (cy + 0.5) * cellSize,
          z: -BOX_LIMIT + (cz + 0.5) * cellSize,
        };
      },
    };
  }

  function buildGridBuckets(cellSize) {
    const info = getGridInfo(cellSize);
    const buckets = new Map();

    for (let i = 0; i < particles.length; i++) {
      const p = particles[i];
      const cx = info.toCellIndex(p.x, info.nx);
      const cy = info.toCellIndex(p.y, info.ny);
      const cz = info.toCellIndex(p.z, info.nz);
      const k = info.key(cx, cy, cz);
      let cell = buckets.get(k);
      if (!cell) {
        cell = { key: k, cx, cy, cz, indices: [] };
        buckets.set(k, cell);
      }
      cell.indices.push(i);
    }

    return { info, buckets };
  }

  function updateGridVisibility() {
    if (!gridHelper) return;
    gridHelper.visible = modeSel.value === "grid";
    if (gridHelper.visible) updateGridCellsVisual();
  }

  function rebuildGridHelper() {
    if (gridHelper) {
      scene.remove(gridHelper);
      gridHelper.traverse((obj) => {
        if (obj.geometry) obj.geometry.dispose();
        if (obj.material) obj.material.dispose();
      });
    }

    gridHelper = new THREE.Group();
    const cellSize = Math.max(10, +cellRange.value || 40);
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
    gridHelper.add(baseLineGrid);

    gridCellCapacity = Math.max(1, Number(nRange?.max) || 0, particles.length);

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

    gridCellFillMesh = new THREE.InstancedMesh(boxGeo, fillMat, gridCellCapacity);
    gridCellFillMesh.instanceMatrix.setUsage(THREE.DynamicDrawUsage);
    gridCellFillMesh.frustumCulled = false;

    gridCellHighlightMesh = new THREE.InstancedMesh(boxGeo.clone(), highlightMat, gridCellCapacity);
    gridCellHighlightMesh.instanceMatrix.setUsage(THREE.DynamicDrawUsage);
    gridCellHighlightMesh.frustumCulled = false;

    gridHelper.add(gridCellFillMesh);
    gridHelper.add(gridCellHighlightMesh);
    scene.add(gridHelper);
    updateGridVisibility();
  }

  function updateGridCellsVisual() {
    if (!gridHelper || modeSel.value !== "grid") return;
    if (particles.length > gridCellCapacity) {
      rebuildGridHelper();
      if (!gridHelper || modeSel.value !== "grid") return;
    }

    const cellSize = Math.max(10, +cellRange.value || 40);
    const { info, buckets } = buildGridBuckets(cellSize);
    const maxOccupancy = Array.from(buckets.values()).reduce(
      (max, cell) => Math.max(max, cell.indices.length),
      1
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
            const cx = scx + dx;
            const cy = scy + dy;
            const cz = scz + dz;
            if (cx < 0 || cy < 0 || cz < 0 || cx >= info.nx || cy >= info.ny || cz >= info.nz) continue;
            neighborKeys.add(info.key(cx, cy, cz));
          }
        }
      }
    }

    let fillIndex = 0;
    let highlightIndex = 0;
    for (const cell of buckets.values()) {

      const center = info.centerOf(cell.cx, cell.cy, cell.cz);
      tmpGridPosition.set(center.x, center.y, center.z);
      tmpGridScale.setScalar(cellSize * 0.94);
      tmpGridMatrix.compose(tmpGridPosition, identityQuat, tmpGridScale);
      gridCellFillMesh.setMatrixAt(fillIndex, tmpGridMatrix);

      const densityT = maxOccupancy <= 1 ? 0 : (cell.indices.length - 1) / (maxOccupancy - 1);
      tmpGridColor.copy(gridSparseColor).lerp(gridDenseColor, densityT);
      gridCellFillMesh.setColorAt(fillIndex, tmpGridColor);
      fillIndex++;

      if (neighborKeys && neighborKeys.has(cell.key)) {
        tmpGridScale.setScalar(cellSize * 1.02);
        tmpGridMatrix.compose(tmpGridPosition, identityQuat, tmpGridScale);
        gridCellHighlightMesh.setMatrixAt(highlightIndex, tmpGridMatrix);
        gridCellHighlightMesh.setColorAt(
          highlightIndex,
          cell.key === selectedCellKey ? gridSelectedCellColor : gridNeighborColor
        );
        highlightIndex++;
      }
    }

    gridCellFillMesh.count = fillIndex;
    gridCellFillMesh.instanceMatrix.needsUpdate = true;
    if (gridCellFillMesh.instanceColor) gridCellFillMesh.instanceColor.needsUpdate = true;

    gridCellHighlightMesh.count = highlightIndex;
    gridCellHighlightMesh.instanceMatrix.needsUpdate = true;
    if (gridCellHighlightMesh.instanceColor) gridCellHighlightMesh.instanceColor.needsUpdate = true;
  }
  function resize() {
    const w = view.clientWidth;
    const h = view.clientHeight;
    renderer.setSize(w, h, false);
    camera.aspect = w / h;
    camera.updateProjectionMatrix();
  }
  window.addEventListener("resize", resize);
  let particles = [];
  let paused = false;
  let selectedIndex = -1;
  let timeScale = 1;
  let isPlanetListHidden = false;
  let energyLoss = 0;
  let energyFrameCounter = 0;
  let focusT = 1;
  let focusStartPos = null;
  let focusStartTarget = null;
  let focusEndTarget = null;

  function isSolarScenario() {
    return Boolean(scenarioSel) && (
      scenarioSel.value === "solar" || scenarioSel.value === "solar_stable"
    );
  }

  function isStableSolarScenario() {
    return Boolean(scenarioSel) && scenarioSel.value === "solar_stable";
  }

  function getGravityConstant() { //如果是穩定的太陽系，就用比較小的 G，讓行星不會飛出去了
    return isStableSolarScenario() ? 0.12 : G;
  }

  function getSofteningValue() {//如果是穩定的太陽系，就用比較小的軟化參數，讓行星之間的引力更真實
    return isStableSolarScenario() ? 0.1 : softening;
  }

  function volumeFromR(r) { //從半徑算出體積
    return (4 / 3) * Math.PI * r * r * r; // 3D sphere volume
  }

  function mulberry32(seed) {//一個簡單的隨機數生成器，給定一個種子，會產生一系列看起來隨機的數字
    return function () {
      let t = (seed += 0x6d2b79f5);//這行代碼對種子進行了一些位運算和乘法，來產生一個新的數字 t。這個過程會改變 t 的值，使得每次調用這個函數時都會得到不同的結果。
      t = Math.imul(t ^ (t >>> 15), t | 1);
      t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
      return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };
  }
  function initParticles(N, seed = 12345) { //隨機生出很多星球
    const rand = mulberry32(seed);
    const box = 350; // Spawn range for initial 3D positions.

    particles = [];
    for (let i = 0; i < N; i++) {
      const r = 2 + rand() * 6; // Random radius.
      const rho = 1;
      const m = rho * volumeFromR(r);
      const hue = rand();
      const sat = 0.45 + rand() * 0.4;
      const light = 0.35 + rand() * 0.25;
      const color = new THREE.Color().setHSL(hue, sat, light).getHex();

      particles.push({
        x: (rand() - 0.5) * box,
        y: (rand() - 0.5) * box,
        z: (rand() - 0.5) * box,

        vx: (rand() - 0.5) * 0.8,
        vy: (rand() - 0.5) * 0.8,
        vz: (rand() - 0.5) * 0.8,

        ax: 0, ay: 0, az: 0,
        r, rho, m,
        color,
        bright: 1,
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
      stable
        ? { name: "Mercury", orbit: 3.9 * 10, mass: 0.055, radius: 1.2, color: 0xa9a9a9 }
        : { name: "Mercury", orbit: 45, mass: 20, radius: 3.2, color: 0xa9a9a9 },
      stable
        ? { name: "Venus", orbit: 7.2 * 10, mass: 0.815, radius: 1.8, color: 0xd9c27f }
        : { name: "Venus", orbit: 70, mass: 35, radius: 4.3, color: 0xd9c27f },
      stable
        ? { name: "Earth", orbit: 10 * 10, mass: 1.0, radius: 1.9, color: 0x4f83ff }
        : { name: "Earth", orbit: 95, mass: 38, radius: 4.5, color: 0x4f83ff },
      stable
        ? { name: "Mars", orbit: 15.2 * 10, mass: 0.107, radius: 1.4, color: 0xc86d4b }
        : { name: "Mars", orbit: 125, mass: 26, radius: 3.8, color: 0xc86d4b },
      stable
        ? { name: "Jupiter", orbit: 52 * 10, mass: 317.8, radius: 4.8, color: 0xd2b48c }
        : { name: "Jupiter", orbit: 175, mass: 420, radius: 10.5, color: 0xd2b48c },
      stable
        ? { name: "Saturn", orbit: 95.8 * 10, mass: 95.2, radius: 4.3, color: 0xe0cf8a }
        : { name: "Saturn", orbit: 235, mass: 300, radius: 9.2, color: 0xe0cf8a },
      stable
        ? { name: "Uranus", orbit: 192 * 10, mass: 14.5, radius: 3.1, color: 0x8fe7ff }
        : { name: "Uranus", orbit: 290, mass: 120, radius: 7.1, color: 0x8fe7ff },
      stable
        ? { name: "Neptune", orbit: 301 * 10, mass: 17.1, radius: 3.0, color: 0x4169e1 }
        : { name: "Neptune", orbit: 340, mass: 130, radius: 7.0, color: 0x4169e1 },
    ];

    particles = [{
      name: "Sun",
      x: 0,
      y: 0,
      z: 0,
      vx: 0,
      vy: 0,
      vz: 0,
      ax: 0,
      ay: 0,
      az: 0,
      r: sunR,
      rho: sunRho,
      m: sunMass,
      color: 0xfff2b3,
      bright: 1.8,
    }];

    let totalPlanetMomentumY = 0;
    for (const def of planetDefs) {
      const speed = stable
        ? Math.sqrt((grav * sunMass) / (60 * def.orbit))
        : Math.sqrt((grav * sunMass) / def.orbit) / 60;
      const rho = def.mass / volumeFromR(def.radius);
      particles.push({
        name: def.name,
        x: def.orbit,
        y: 0,
        z: 0,
        vx: 0,
        vy: speed,
        vz: 0,
        ax: 0,
        ay: 0,
        az: 0,
        r: def.radius,
        rho,
        m: def.mass,
        color: def.color,
        bright: 1.1,
      });
      totalPlanetMomentumY += def.mass * speed;
    }

    if (stable) {
      particles[0].vy = -totalPlanetMomentumY / sunMass;
    }

    selectedIndex = -1;
    editorBox.style.display = "none";
  }
  const spheres = []; // sphere mesh list
  const trailLines = [];
  const TRAIL_POINTS = 40;
  const baseMat = new THREE.MeshStandardMaterial({ color: 0x000000 });
  const selMat = new THREE.MeshStandardMaterial({ color: 0xffb020 });

  function rebuildSpheres() {//把資料變成真的 3D 球
    for (const s of spheres) {
      scene.remove(s);
      s.geometry.dispose();
      if (s.userData.baseMat) s.userData.baseMat.dispose();
    }
    spheres.length = 0;
    for (let i = 0; i < particles.length; i++) {
      const p = particles[i];
      const geo = new THREE.SphereGeometry(p.r, 18, 18); 
      /* 後面的 18, 18 是幹嘛？ 3D 球其實不是完美圓球。 它是很多小三角形拼起來的。
      切越少 → 球看起來比較粗糙
      切越多 → 球看起來越圓，但比較耗效能*/

      const particleMat = baseMat.clone();
      particleMat.transparent = true;
      particleMat.opacity = 1;
      const mesh = new THREE.Mesh(geo, particleMat);
      mesh.userData.baseMat = particleMat;
      mesh.userData.index = i; // Keep particle index for raycast selection.
      mesh.position.set(p.x, p.y, p.z);
      spheres.push(mesh);
      scene.add(mesh);
    }
  }
/*
particle = 星球資料表
SphereGeometry = 球的形狀
Mesh = 真的放到 3D 世界的球 
*/

  function clearTrails() {
    for (const t of trailLines) {
      scene.remove(t.line);
      t.line.geometry.dispose();
      t.line.material.dispose();
    }
    trailLines.length = 0;
  }

  function rebuildTrails() {
    clearTrails();
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
      scene.add(line);

      trailLines.push({ line, positions, attr });
    }
  }

  function updateTrails(advance = true) {
    for (let i = 0; i < particles.length; i++) {
      const p = particles[i];
      const t = trailLines[i];
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

  function updateSpheresTransform() {
    for (let i = 0; i < particles.length; i++) {
      const p = particles[i];
      const mesh = spheres[i];
      mesh.position.set(p.x, p.y, p.z);
      const currentR = mesh.geometry.parameters.radius;
      if (Math.abs(currentR - p.r) > 0.001) {
        mesh.geometry.dispose();
        mesh.geometry = new THREE.SphereGeometry(p.r, 18, 18);
      }

      if (i === selectedIndex) {
        mesh.material = selMat;
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
  const G = 35;
  const softening = 25;
  const dt = 0.016;
  /*
    1 / 60 秒 因為螢幕常見是 60 FPS。
    1 秒 60 張畫面
    1 張畫面約 0.0167 秒
  */
  const damping = 0.997;

  function getDampingFactor() {
    return isStableSolarScenario() ? 1 : damping;
  }

  function getEffectiveEnergyLoss() {
    return isStableSolarScenario() ? 0 : energyLoss;
  }

  function updateSpeedLabel() {
    speedLabel.textContent = `${Number(speedRange.value).toFixed(2)}x`;
  }

  function updateLossLabel() {
    const lossValue = isStableSolarScenario() ? 0 : Number(lossRange.value);
    lossLabel.textContent = lossValue.toFixed(3);
  }

  function addPlanetFromInputs() {
    // Check if all required input elements exist
    if (!addX || !addY || !addZ || !addVx || !addVy || !addVz || !addR || !addRho) return;
    // Get and validate input values
    const x = Number(addX.value);
    const y = Number(addY.value);
    const z = Number(addZ.value);
    const vx = Number(addVx.value);
    const vy = Number(addVy.value);
    const vz = Number(addVz.value);
    const r = Math.max(0.1, Number(addR.value));
    const rho = Math.max(0.0001, Number(addRho.value));
    // Calculate mass from density and radius
    const m = rho * volumeFromR(r);
    const color = addColor ? parseInt(addColor.value.replace("#", ""), 16) : 0x000000;
    const bright = addBright ? Math.max(0.2, Number(addBright.value)) : 1;

    // Add new particle to the array
    particles.push({
      x: Number.isFinite(x) ? x : 0, //新增星球的 x 輸入框是空的，轉成 Number 可能會出問題。
      y: Number.isFinite(y) ? y : 0,
      z: Number.isFinite(z) ? z : 0,
      vx: Number.isFinite(vx) ? vx : 0,
      vy: Number.isFinite(vy) ? vy : 0,
      vz: Number.isFinite(vz) ? vz : 0,
      ax: 0, ay: 0, az: 0,
      r, rho, m,
      color,
      bright,
    });

    // Set the newly added particle as selected
    selectedIndex = particles.length - 1;
    // Update the editor UI with the new particle's data
    syncEditorFromParticle(particles[selectedIndex], selectedIndex);

    // Update the particle count slider if it exists
    if (nRange) {
      // If current particle count exceeds slider max, update max
      if (particles.length > Number(nRange.max)) {
        nRange.max = String(particles.length);
      }
      // Set slider value to current particle count
      nRange.value = String(particles.length);
      // Update the label text
      nLabel.textContent = String(particles.length);
    }

    // Rebuild 3D spheres for rendering
    rebuildSpheres();
    // Rebuild particle trails
    rebuildTrails();
    // Update the planet list UI
    renderPlanetList();
  }

  function stepNaive3D() {
    // Perform one full timestep using naive N-body simulation
    let ops = computeAccelerationsNaive();
    leapfrogKickDrift();
    ops += computeAccelerationsNaive();
    leapfrogKick();
    return ops;
  }
  // 3D uniform grid: only evaluate nearby cells (3x3x3 neighborhood)
  function stepGrid3D(cellSize) {
    let ops = computeAccelerationsGrid(cellSize);
    leapfrogKickDrift();
    ops += computeAccelerationsGrid(cellSize);
    leapfrogKick();
    return ops;
  }
  function computeAccelerationsNaive() {
    // Get gravity and softening constants
    const grav = getGravityConstant();
    const soften = getSofteningValue();
    let ops = 0;
    // Reset accelerations for all particles
    for (const p of particles) { p.ax = p.ay = p.az = 0; }

    const N = particles.length;
    // Calculate forces between all pairs of particles
    for (let i = 0; i < N; i++) {
      const a = particles[i];
      for (let j = i + 1; j < N; j++) {
        const b = particles[j];

        // Calculate distance vector
        const dx = b.x - a.x;
        const dy = b.y - a.y;
        const dz = b.z - a.z;

        // Calculate squared distance with softening
        const dist2 = dx*dx + dy*dy + dz*dz + soften;
        const inv = 1 / Math.sqrt(dist2);  //距離的反比，後面會用到
        /*
        如果兩顆星球太靠近，
        吸引力可能變超大，
        程式會爆衝，
        所以加一點緩衝。
        */

        // Calculate gravitational force  F = G × m1 × m2 / r²   
        // F = m × a
        const f = grav * inv * inv;
        const fx = f * dx * inv;
        const fy = f * dy * inv;
        const fz = f * dz * inv;

        // Apply force to both particles (Newton's 3rd law)
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
    for (const p of particles) { p.ax = p.ay = p.az = 0; }

    const { info, buckets } = buildGridBuckets(cellSize);

    for (const cellA of buckets.values()) {
      const listA = cellA.indices;
      const idA = info.idOf(cellA.cx, cellA.cy, cellA.cz);

      for (let dz = -1; dz <= 1; dz++) {
        for (let dy = -1; dy <= 1; dy++) {
          for (let dx = -1; dx <= 1; dx++) {
            const nxCell = cellA.cx + dx;
            const nyCell = cellA.cy + dy;
            const nzCell = cellA.cz + dz;
            if (
              nxCell < 0 || nyCell < 0 || nzCell < 0 ||
              nxCell >= info.nx || nyCell >= info.ny || nzCell >= info.nz
            ) {
              continue;
            }

            const cellB = buckets.get(info.key(nxCell, nyCell, nzCell));
            if (!cellB) continue;
            const listB = cellB.indices;

            const idB = info.idOf(nxCell, nyCell, nzCell);
            if (idB < idA) continue; // avoid duplicate inter-cell pairs

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

  /*leapfrogKickDrift
    位置會影響吸引力
    吸引力會影響加速度
    加速度會影響速度
    速度會影響位置
    位置變了又會影響吸引力
   */
  function leapfrogKickDrift() {
    // Leapfrog integration: update velocity (kick) then position (drift)
    const dtScaled = dt * timeScale;
    const half = dtScaled * 0.5;
    const useBounds = !isStableSolarScenario();
    for (const p of particles) {
      // Update velocity by half timestep using current acceleration
      p.vx += p.ax * half; p.vy += p.ay * half; p.vz += p.az * half;

      // Update position using updated velocity (scaled by 60 for time units)
      p.x += p.vx * 60 * dtScaled; // 因為我們的速度單位是「每秒」，而 dt 是以秒為單位的，所以乘以 60 來對齊時間尺度。
      p.y += p.vy * 60 * dtScaled; //dtScaled = dt × speed -> dt = 0.016 -> 60 × 0.016 ≈ 1
      p.z += p.vz * 60 * dtScaled;

      const r = p.r || 2;

      if (useBounds) { //如果星球撞到左邊牆壁，就把 x 方向速度反過來
        // Bounce off 3D box boundaries (elastic collision)
        if (p.x < -BOX_LIMIT + r) { p.x = -BOX_LIMIT + r; p.vx *= -1; }
        if (p.x >  BOX_LIMIT - r) { p.x =  BOX_LIMIT - r; p.vx *= -1; }
        if (p.y < -BOX_LIMIT + r) { p.y = -BOX_LIMIT + r; p.vy *= -1; }
        if (p.y >  BOX_LIMIT - r) { p.y =  BOX_LIMIT - r; p.vy *= -1; }
        if (p.z < -BOX_LIMIT + r) { p.z = -BOX_LIMIT + r; p.vz *= -1; }
        if (p.z >  BOX_LIMIT - r) { p.z =  BOX_LIMIT - r; p.vz *= -1; }
      }
    }
  }

  function leapfrogKick() {
    // Complete the Leapfrog integration: update velocity by another half timestep
    const dtScaled = dt * timeScale;
    const half = dtScaled * 0.5;
    const lossFactor = Math.max(0, 1 - getEffectiveEnergyLoss() * dtScaled * 60);
    const dampingFactor = getDampingFactor();
    for (const p of particles) {
      // Update velocity with damping and energy loss
      p.vx = (p.vx + p.ax * half) * dampingFactor * lossFactor;
      p.vy = (p.vy + p.ay * half) * dampingFactor * lossFactor;
      p.vz = (p.vz + p.az * half) * dampingFactor * lossFactor;
    }
  }

  function handleCollisionsAndMerge() {
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

          // Conservation of momentum
          const vx = (a.vx * mA + b.vx * mB) / mT;   //p = m × v → v = p / m
          const vy = (a.vy * mA + b.vy * mB) / mT;
          const vz = (a.vz * mA + b.vz * mB) / mT;

          // Center of mass position
          const x = (a.x * mA + b.x * mB) / mT;
          const y = (a.y * mA + b.y * mB) / mT;
          const z = (a.z * mA + b.z * mB) / mT;

          // Keep density as mass-weighted average
          const rhoA = a.rho ?? 1;
          const rhoB = b.rho ?? 1;
          const rho = (rhoA * mA + rhoB * mB) / mT;
          const r = Math.cbrt((3 * mT) / (4 * Math.PI * rho));

          // 融合後的顏色取質量較大的星球的顏色
          const color = (mA > mB) ? a.color : b.color;
          const bright = (mA > mB) ? (a.bright ?? 1) : (b.bright ?? 1);

          const keep = i;
          const drop = j;

          particles[keep] = {
            x, y, z,
            vx, vy, vz,
            ax: 0, ay: 0, az: 0,
            r, rho, m: mT,
            color,
            bright,
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

  function updateScenarioUI() {
    const isSolar = isSolarScenario();
    nRange.disabled = Boolean(isSolar);
    nLabel.textContent = isSolar ? "9" : nRange.value;
    lossRange.disabled = isStableSolarScenario();
    if (isStableSolarScenario()) lossRange.value = "0";
    updateLossLabel();
  }

  function updatePlanetListVisibility() {
    if (!planetListBody || !planetListToggleBtn) return;
    planetListBody.style.display = isPlanetListHidden ? "none" : "block";
    planetListToggleBtn.textContent = isPlanetListHidden ? "Show" : "Hide";
  }

  function togglePlanetListPanel() {
    isPlanetListHidden = !isPlanetListHidden;
    updatePlanetListVisibility();
  }

  const raycaster = new THREE.Raycaster(); //用來把滑鼠點擊位置轉換成 3D 空間中的射線，然後檢測這條射線和哪些物體相交，從而實現點擊選擇星球的功能。
  const mouse = new THREE.Vector2();

  function focusCameraOnPlanet(p) {
    // 設定相機目標為星球位置
    controls.target.set(p.x, p.y, p.z);
    
    // 設定相機位置為星球上方一定距離
    // 你可以調整這些數值來改變視角
    const distance = 300;  // 相機距離星球的距離
    const height = 150;    // 相機的高度偏移
    
    camera.position.set(
      p.x,           // X 位置與星球相同
      p.y + height,  // Y 位置在星球上方
      p.z + distance // Z 位置在星球前方
    );
    
    // 重置平滑過渡變數
    focusT = 1;
  }

  let focusEndPos = null;

  function updateCameraFocus() {
    if (focusT >= 1 || !focusEndPos || !focusEndTarget) return;
    focusT = Math.min(1, focusT + 0.08);
    camera.position.lerpVectors(focusStartPos, focusEndPos, focusT);
    controls.target.lerpVectors(focusStartTarget, focusEndTarget, focusT);
  }

  renderer.domElement.addEventListener("pointerdown", (e) => {
    const rect = renderer.domElement.getBoundingClientRect();
    mouse.x = ((e.clientX - rect.left) / rect.width) * 2 - 1;
    mouse.y = -(((e.clientY - rect.top) / rect.height) * 2 - 1);

/*1. 先取得 3D 畫布在網頁上的位置和大小。
2. 把滑鼠在瀏覽器的位置，換成滑鼠在畫布裡的位置。
3. 把 x 從 0~寬度，轉成 -1~+1。
4. 把 y 從 0~高度，轉成 +1~-1。
5. 這樣 Three.js 的 Raycaster 才知道你點的是 3D 畫面的哪個方向。
 */
    raycaster.setFromCamera(mouse, camera);
    const hits = raycaster.intersectObjects(spheres, false);

    if (hits.length > 0) {
      const idx = hits[0].object.userData.index;
      selectedIndex = idx;
      // 只有管理員才能編輯星球
      if (isAdmin) {
        syncEditorFromParticle(particles[idx], idx);
      }
      focusCameraOnPlanet(particles[idx]);
    } else {
      selectedIndex = -1;
      editorBox.style.display = "none";
    }
    renderPlanetList();
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
  let lastNaiveOpsBaseline = null;
  let lastNForBaseline = null;

  function ensureBaseline(N, ops, mode) {
    if (mode === "naive") {
      lastNaiveOpsBaseline = ops;
      lastNForBaseline = N;
    }
  }

  function computeEI(N, ops) {
    if (lastNaiveOpsBaseline == null || lastNForBaseline !== N) return "-";
    return ((ops / lastNaiveOpsBaseline) * 100).toFixed(1) + "%";
  }

  function computeTimeComplexity(mode, N, ops) {
    if (mode === "naive") return "O(N^2)";
    const k = N > 0 ? (2 * ops) / N : 0;
    return `~O(N·k), k≈${k.toFixed(1)}`;
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

  function loop() {
    // Get current particle count and settings
    const N = particles.length;
    const cellSize = +cellRange.value;
    timeScale = Math.max(0.01, +speedRange.value || 1);
    energyLoss = Math.max(0, +lossRange.value || 0);

    // 如果有選定的星球，讓相機跟隨
    if (selectedIndex >= 0 && selectedIndex < particles.length) {
      const p = particles[selectedIndex];
      const distance = 300;
      const height = 150;
      
      camera.position.set(
        p.x,
        p.y + height,
        p.z + distance
      );
      controls.target.set(p.x, p.y, p.z);
    }

    // Measure performance start time
    const t0 = performance.now();
    let ops = 0;

    // If not paused, perform physics simulation
    if (!paused) {
      // Choose simulation method based on mode
      if (modeSel.value === "naive") ops = stepNaive3D();
      else ops = stepGrid3D(cellSize);
      // Handle collisions and merging if occurred
      if (handleCollisionsAndMerge()) {
        rebuildSpheres();
        rebuildTrails();
        renderPlanetList();
      }
    }

    // Measure performance end time
    const t1 = performance.now();
    const ms = t1 - t0;

    // Update performance metrics
    ensureBaseline(N, ops, modeSel.value);
    updateTrails(!paused);
    updateSpheresTransform();
    updateCameraFocus();
    controls.update();
    if (gridHelper && gridHelper.visible) updateGridCellsVisual();
    renderer.render(scene, camera);

    // Update UI display elements
    msEl.textContent = paused ? "paused" : ms.toFixed(2);
    opsEl.textContent = paused ? "-" : ops.toString();
    eiEl.textContent = paused ? "-" : (modeSel.value === "naive" ? "100%" : computeEI(N, ops));
    tcEl.textContent = paused ? "-" : computeTimeComplexity(modeSel.value, N, ops);
    if (!paused) {
      energyFrameCounter = (energyFrameCounter + 1) % 10;
      if (energyFrameCounter === 0) {
        const totalEnergy = computeTotalEnergy();
        energyEl.textContent = totalEnergy.toFixed(2);
      }
    }

    // Schedule next frame
    requestAnimationFrame(loop);
  }

  // ======= reset / pause =======
  function resetAll() {
    // Update cell size label
    cellLabel.textContent = cellRange.value;
    // Update scenario-specific UI elements
    updateScenarioUI();

    // Initialize particles based on scenario
    if (isSolarScenario()) {
      initSolarSystemPreset();
    } else {
      const N = +nRange.value;
      initParticles(N, 12345);
    }

    // Rebuild visual elements
    rebuildSpheres();
    rebuildTrails();
    renderPlanetList();

    // Reset performance baseline
    lastNaiveOpsBaseline = null;
    lastNForBaseline = null;
  }

  nRange.addEventListener("input", () => {
    if (isSolarScenario()) return;
    nLabel.textContent = nRange.value;
  });
  cellRange.addEventListener("input", () => {
    cellLabel.textContent = cellRange.value;
    rebuildGridHelper();
  });
  speedRange.addEventListener("input", updateSpeedLabel);
  lossRange.addEventListener("input", updateLossLabel);
  modeSel.addEventListener("change", updateGridVisibility);
  if (scenarioSel) scenarioSel.addEventListener("change", resetAll);
  resetBtn.addEventListener("click", resetAll);
  if (addPlanetBtn) addPlanetBtn.addEventListener("click", addPlanetFromInputs);
  if (planetListToggleBtn) planetListToggleBtn.addEventListener("click", togglePlanetListPanel);
  if (planetListEl) {
    planetListEl.addEventListener("click", (e) => {
      const btn = e.target.closest("[data-planet-index]");
      if (!btn) return;
      const idx = Number(btn.dataset.planetIndex);
      if (!Number.isFinite(idx) || idx < 0 || idx >= particles.length) return; 

      selectedIndex = idx;
      // 只有管理員才能編輯星球
      if (isAdmin) {
        syncEditorFromParticle(particles[idx], idx);
      }
      renderPlanetList();
    });
  }

  pauseBtn.addEventListener("click", () => {
    paused = !paused;
    pauseBtn.textContent = paused ? "Resume" : "Pause";
  });

  // ======= start =======
  updateSpeedLabel();
  updateLossLabel();
  updatePlanetListVisibility();
  updateScenarioUI();
  rebuildGridHelper();
  resize();
  resetAll();
  loop();
})();











