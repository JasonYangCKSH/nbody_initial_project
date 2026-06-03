(() => {
  const canvas = document.getElementById("c");
  const ctx = canvas.getContext("2d");

  const modeSel = document.getElementById("mode");
  const nRange = document.getElementById("nRange");
  const nLabel = document.getElementById("nLabel");
  const cellRange = document.getElementById("cellRange");
  const cellLabel = document.getElementById("cellLabel");
  const resetBtn = document.getElementById("resetBtn");
  const pauseBtn = document.getElementById("pauseBtn");

  const msEl = document.getElementById("ms");
  const opsEl = document.getElementById("ops");
  const eiEl = document.getElementById("ei");
  const editorBox = document.getElementById("editorBox");
  const selName = document.getElementById("selName");
  const rInput = document.getElementById("rInput");
  const rhoInput = document.getElementById("rhoInput");
  const mInput = document.getElementById("mInput");
  const rVal = document.getElementById("rVal");
  const rhoVal = document.getElementById("rhoVal");
  const mVal = document.getElementById("mVal");

  let selectedIndex = -1;

  function areaFromR(r) {
    return Math.PI * r * r; // 2D ?Ｙ?
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
  }

  function applyRhoAndRToMass(p) {
    const r = Math.max(0.1, Number(rInput.value));
    const rho = Math.max(0.0001, Number(rhoInput.value));

    p.r = r;
    p.rho = rho;
    p.m = rho * areaFromR(r);

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

    const rho = m / areaFromR(r);
    p.rho = rho;

    rVal.textContent = r.toFixed(1);
    mVal.textContent = m.toFixed(2);
    rhoInput.value = rho.toFixed(1);
    rhoVal.textContent = Number(rhoInput.value).toFixed(1);
  }

  function resize() {
    const dpr = window.devicePixelRatio || 1;
    canvas.width = Math.floor(canvas.clientWidth * dpr);
    canvas.height = Math.floor(canvas.clientHeight * dpr);
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  }
  window.addEventListener("resize", resize);
  let particles = [];
  let paused = false;
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
    const W = canvas.clientWidth;
    const H = canvas.clientHeight;

    particles = [];
    for (let i = 0; i < N; i++) {
      const r = 1.5 + rand() * 3.5; // 1.5 ~ 5
      const rho = 1;
      const m = rho * areaFromR(r);

      particles.push({
        x: rand() * W,
        y: rand() * H,
        vx: (rand() - 0.5) * 0.6,
        vy: (rand() - 0.5) * 0.6,
        ax: 0,
        ay: 0,
        r,
        rho,
        m,
      });
    }
    selectedIndex = -1;
    editorBox.style.display = "none";
  }
  const G = 30;
  const softening = 20; // Avoid extreme force at very small distance.
  const dt = 0.016;
  function stepNaive(W, H) {
    let ops = 0;
    for (const p of particles) {
      p.ax = 0;
      p.ay = 0;
    }

    const N = particles.length;
    for (let i = 0; i < N; i++) {
      const a = particles[i];
      for (let j = i + 1; j < N; j++) {
        const b = particles[j];

        const dx = b.x - a.x;
        const dy = b.y - a.y;

        const dist2 = dx * dx + dy * dy + softening;
        const inv = 1 / Math.sqrt(dist2);
        const f = G * inv * inv;
        const fx = f * dx * inv; // = G * dx / dist^3
        const fy = f * dy * inv;

        a.ax += fx * b.m;
        a.ay += fy * b.m;
        b.ax -= fx * a.m;
        b.ay -= fy * a.m;

        ops++;
      }
    }

    integrate(W, H);
    return ops;
  }
  function stepGrid(W, H, cellSize) {
    let ops = 0;
    for (const p of particles) {
      p.ax = 0;
      p.ay = 0;
    }

    const cols = Math.max(1, Math.floor(W / cellSize));
    const rows = Math.max(1, Math.floor(H / cellSize));
    const buckets = new Map();

    function key(cx, cy) {
      return cx + "," + cy;
    }

    // 1) Put particles into grid buckets.
    for (let i = 0; i < particles.length; i++) {
      const p = particles[i];
      const cx = Math.min(cols - 1, Math.max(0, Math.floor(p.x / cellSize)));
      const cy = Math.min(rows - 1, Math.max(0, Math.floor(p.y / cellSize)));
      const k = key(cx, cy);
      if (!buckets.has(k)) buckets.set(k, []);
      buckets.get(k).push(i);
    }
    for (let cy = 0; cy < rows; cy++) {
      for (let cx = 0; cx < cols; cx++) {
        const listA = buckets.get(key(cx, cy));
        if (!listA) continue;

        for (let ny = cy - 1; ny <= cy + 1; ny++) {
          for (let nx = cx - 1; nx <= cx + 1; nx++) {
            if (nx < 0 || ny < 0 || nx >= cols || ny >= rows) continue;

            const listB = buckets.get(key(nx, ny));
            if (!listB) continue;

            const sameCell = nx === cx && ny === cy;

            for (let ia = 0; ia < listA.length; ia++) {
              const i = listA[ia];
              const a = particles[i];

              let jbStart = 0;
              if (sameCell) jbStart = ia + 1; // Skip duplicate pairs in same cell.

              for (let jb = jbStart; jb < listB.length; jb++) {
                const j = listB[jb];
                const b = particles[j];

                const dx = b.x - a.x;
                const dy = b.y - a.y;

                const dist2 = dx * dx + dy * dy + softening;
                const inv = 1 / Math.sqrt(dist2);

                const f = G * inv * inv;
                const fx = f * dx * inv;
                const fy = f * dy * inv;
                a.ax += fx * b.m;
                a.ay += fy * b.m;
                b.ax -= fx * a.m;
                b.ay -= fy * a.m;

                ops++;
              }
            }
          }
        }
      }
    }

    integrate(W, H);
    return ops;
  }

  function integrate(W, H) {


  const damping = 0.997;

  for (const p of particles) {
    p.vx += p.ax * dt;
    p.vy += p.ay * dt;

    p.vx *= damping;
    p.vy *= damping;

    p.x += p.vx * 60 * dt;
    p.y += p.vy * 60 * dt;

    const r = p.r || 2;
    if (p.x < r) { p.x = r; p.vx *= -1; }
    if (p.x > W - r) { p.x = W - r; p.vx *= -1; }
    if (p.y < r) { p.y = r; p.vy *= -1; }
    if (p.y > H - r) { p.y = H - r; p.vy *= -1; }
  }


  }

  function draw(W, H, cellSize) {
    ctx.clearRect(0, 0, W, H);
    if (modeSel.value === "grid") {
      ctx.save();
      ctx.globalAlpha = 0.15;
      ctx.strokeStyle = "#000";
      ctx.lineWidth = 1;

      ctx.beginPath();
      for (let x = 0; x <= W; x += cellSize) {
        ctx.moveTo(x + 0.5, 0);
        ctx.lineTo(x + 0.5, H);
      }
      for (let y = 0; y <= H; y += cellSize) {
        ctx.moveTo(0, y + 0.5);
        ctx.lineTo(W, y + 0.5);
      }
      ctx.stroke();
      ctx.restore();
    }
    ctx.save();
    ctx.fillStyle = "#959595ff";
    ctx.beginPath();
    for (const p of particles) {
      const r = p.r || 2;
      ctx.moveTo(p.x + r, p.y);
      ctx.arc(p.x, p.y, r, 0, Math.PI * 2);
    }
    ctx.fill();
    ctx.restore();
    if (selectedIndex >= 0) {
      const p = particles[selectedIndex];
      const r = p.r || 2;
      ctx.save();
      ctx.globalAlpha = 0.9;
      ctx.lineWidth = 2;
      ctx.beginPath();
      ctx.arc(p.x, p.y, r + 3, 0, Math.PI * 2);
      ctx.stroke();
      ctx.restore();
    }
  }
  canvas.addEventListener("click", (e) => {
    const rect = canvas.getBoundingClientRect();
    const mx = e.clientX - rect.left;
    const my = e.clientY - rect.top;

    selectedIndex = -1;
    for (let i = particles.length - 1; i >= 0; i--) {
      const p = particles[i];
      const r = p.r || 2;
      const dx = mx - p.x;
      const dy = my - p.y;
      if (dx * dx + dy * dy <= r * r) {
        selectedIndex = i;
        syncEditorFromParticle(p, i);
        break;
      }
    }

    if (selectedIndex === -1) {
      editorBox.style.display = "none";
    }
  });
  rInput.addEventListener("input", () => {
    if (selectedIndex < 0) return;
    applyRhoAndRToMass(particles[selectedIndex]);
  });
  rhoInput.addEventListener("input", () => {
    if (selectedIndex < 0) return;
    applyRhoAndRToMass(particles[selectedIndex]);
  });
  mInput.addEventListener("input", () => {
    if (selectedIndex < 0) return;
    applyMassToRho(particles[selectedIndex]);
  });

  async function stepBackend2D(W, H, cellSize) {
    const payload = {
      dimension: "2d",
      particles,
      width: W,
      height: H,
      mode: modeSel.value,
      cellSize,
      dt: 0.016,
      G: 30,
      softening: 20,
    };
    try {
      const result = await window.stepSimulation(payload);
      if (Array.isArray(result.particles)) {
        particles = result.particles;
      }
      return Number(result.ops) || 0;
    } catch (error) {
      console.error("2D backend step failed, falling back to local compute:", error);
      return modeSel.value === "naive" ? stepNaive(W, H) : stepGrid(W, H, cellSize);
    }
  }

  // ======= UI & Loop =======
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

  async function loop() {
    const W = canvas.clientWidth;
    const H = canvas.clientHeight;
    const N = particles.length;
    const cellSize = +cellRange.value;

    const t0 = performance.now();
    let ops = 0;

    if (!paused) {
      ops = await stepBackend2D(W, H, cellSize);
    }

    const t1 = performance.now();
    const ms = t1 - t0;

    ensureBaseline(N, ops, modeSel.value);

    draw(W, H, cellSize);

    msEl.textContent = paused ? "paused" : ms.toFixed(2);
    opsEl.textContent = paused ? "-" : ops.toString();
    eiEl.textContent = paused
      ? "-"
      : modeSel.value === "naive"
      ? "100%"
      : computeEI(N, ops);

    requestAnimationFrame(loop);
  }

  function resetAll() {
    const N = +nRange.value;
    nLabel.textContent = N;
    cellLabel.textContent = cellRange.value;

    initParticles(N, 12345);
    lastNaiveOpsBaseline = null;
    lastNForBaseline = null;
  }

  nRange.addEventListener("input", () => {
    nLabel.textContent = nRange.value;
  });
  cellRange.addEventListener("input", () => {
    cellLabel.textContent = cellRange.value;
  });

  resetBtn.addEventListener("click", resetAll);

  modeSel.addEventListener("change", () => {
  });

  async function loadBackendConfig() {
    try {
      const config = await window.fetchConfig();
      if (config?.default2D) {
        modeSel.value = config.default2D.mode || modeSel.value;
        nRange.value = config.default2D.n || nRange.value;
        cellRange.value = config.default2D.cellSize || cellRange.value;
        nLabel.textContent = nRange.value;
        cellLabel.textContent = cellRange.value;
      }
    } catch (error) {
      console.warn("Failed to load backend config:", error);
    }
  }

  loadBackendConfig();

  pauseBtn.addEventListener("click", () => {
    paused = !paused;
    pauseBtn.textContent = paused ? "Resume" : "Pause";
  });

  resize();
  resetAll();
  loop();
})();

