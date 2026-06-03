const express = require("express");
const path = require("path");

const app = express();
const port = process.env.PORT || 3000;

app.use(express.json({ limit: "8mb" }));
app.use((req, res, next) => {
  res.header("Access-Control-Allow-Origin", "*");
  res.header("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  res.header("Access-Control-Allow-Headers", "Content-Type");
  if (req.method === "OPTIONS") return res.sendStatus(204);
  next();
});
app.get("/", (req, res) => {
  res.sendFile(path.join(__dirname, "../frontend/three.html"));
});
app.use(express.static(path.join(__dirname, "../frontend")));
app.use(express.static(path.join(__dirname), { dotfiles: "ignore" }));

app.get("/api/config", (req, res) => {
  res.json({
    default2D: { n: 120, mode: "naive", cellSize: 40 },
    default3D: { n: 120, mode: "naive", cellSize: 40, speed: 1, loss: 0 },
    modes: ["naive", "grid"],
    cellRange: { min: 10, max: 120, step: 5 },
    speedRange: { min: 0.1, max: 4, step: 0.1 },
    lossRange: { min: 0, max: 0.05, step: 0.001 },
  });
});

app.post("/api/step", (req, res) => {
  try {
    const payload = req.body || {};
    const dimension = payload.dimension === "3d" ? "3d" : "2d";
    const response = dimension === "3d" ? computeNextStep3D(payload) : computeNextStep2D(payload);
    res.json(response);
  } catch (error) {
    console.error(error);
    res.status(500).json({ error: error.message || "Server error" });
  }
});

app.listen(port, () => {
  console.log(`Server running on http://localhost:${port}`);
});

function computeNextStep2D(payload) {
  const particles = Array.isArray(payload.particles) ? payload.particles.map(copyParticle2D) : [];
  const W = Number(payload.width) || 800;
  const H = Number(payload.height) || 600;
  const mode = payload.mode === "grid" ? "grid" : "naive";
  const cellSize = Number(payload.cellSize) || 40;
  const dt = Number(payload.dt) || 0.016;
  const G = Number(payload.G) || 30;
  const softening = Number(payload.softening) || 20;
  const damping = 0.997;

  let ops = 0;
  if (mode === "naive") {
    ops = stepNaive2D(particles, W, H, G, softening);
  } else {
    ops = stepGrid2D(particles, W, H, cellSize, G, softening);
  }
  integrate2D(particles, W, H, dt, damping);
  return { particles, ops };
}

function computeNextStep3D(payload) {
  const particles = Array.isArray(payload.particles) ? payload.particles.map(copyParticle3D) : [];
  const mode = payload.mode === "grid" ? "grid" : "naive";
  const cellSize = Number(payload.cellSize) || 40;
  const timeScale = Number(payload.timeScale) || 1;
  const energyLoss = Number(payload.energyLoss) || 0;
  const stable = payload.stable === true;
  const dt = Number(payload.dt) || 0.016;
  const G = stable ? 0.12 : Number(payload.G || 35);
  const softening = stable ? 0.1 : Number(payload.softening || 25);
  const damping = 0.997;
  const BOX_LIMIT = 380;

  let ops = 0;
  if (mode === "naive") {
    ops = computeAccelerationsNaive3D(particles, G, softening);
  } else {
    ops = computeAccelerationsGrid3D(particles, cellSize, BOX_LIMIT, G, softening);
  }

  leapfrogKickDrift3D(particles, dt, timeScale, BOX_LIMIT, stable);

  if (mode === "naive") {
    ops += computeAccelerationsNaive3D(particles, G, softening);
  } else {
    ops += computeAccelerationsGrid3D(particles, cellSize, BOX_LIMIT, G, softening);
  }

  leapfrogKick3D(particles, dt, timeScale, energyLoss, damping, stable);

  const merged = handleCollisionsAndMerge3D(particles, stable);
  return { particles, ops, merged };
}

function copyParticle2D(p) {
  return {
    x: Number(p.x) || 0,
    y: Number(p.y) || 0,
    vx: Number(p.vx) || 0,
    vy: Number(p.vy) || 0,
    ax: Number(p.ax) || 0,
    ay: Number(p.ay) || 0,
    r: Number(p.r) || 2,
    rho: Number(p.rho) || 1,
    m: Number(p.m) || 1,
  };
}

function copyParticle3D(p) {
  return {
    name: p.name,
    x: Number(p.x) || 0,
    y: Number(p.y) || 0,
    z: Number(p.z) || 0,
    vx: Number(p.vx) || 0,
    vy: Number(p.vy) || 0,
    vz: Number(p.vz) || 0,
    ax: Number(p.ax) || 0,
    ay: Number(p.ay) || 0,
    az: Number(p.az) || 0,
    r: Number(p.r) || 2,
    rho: Number(p.rho) || 1,
    m: Number(p.m) || 1,
    color: Number(p.color) || 0,
    bright: Number(p.bright) || 1,
  };
}

function stepNaive2D(particles, W, H, G, softening) {
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
      const fx = f * dx * inv;
      const fy = f * dy * inv;
      a.ax += fx * b.m;
      a.ay += fy * b.m;
      b.ax -= fx * a.m;
      b.ay -= fy * a.m;
      ops++;
    }
  }
  return ops;
}

function stepGrid2D(particles, W, H, cellSize, G, softening) {
  let ops = 0;
  for (const p of particles) {
    p.ax = 0;
    p.ay = 0;
  }
  const cols = Math.max(1, Math.floor(W / cellSize));
  const rows = Math.max(1, Math.floor(H / cellSize));
  const buckets = new Map();
  const key = (cx, cy) => `${cx},${cy}`;

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
            if (sameCell) jbStart = ia + 1;
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
  return ops;
}

function integrate2D(particles, W, H, dt, damping) {
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

function computeAccelerationsNaive3D(particles, grav, soften) {
  let ops = 0;
  for (const p of particles) {
    p.ax = 0;
    p.ay = 0;
    p.az = 0;
  }
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
      a.ax += fx * b.m;
      a.ay += fy * b.m;
      a.az += fz * b.m;
      b.ax -= fx * a.m;
      b.ay -= fy * a.m;
      b.az -= fz * a.m;
      ops++;
    }
  }
  return ops;
}

function computeAccelerationsGrid3D(particles, cellSize, boxLimit, grav, soften) {
  let ops = 0;
  for (const p of particles) {
    p.ax = 0;
    p.ay = 0;
    p.az = 0;
  }
  const span = boxLimit * 2;
  const nx = Math.max(1, Math.floor(span / cellSize));
  const ny = Math.max(1, Math.floor(span / cellSize));
  const nz = Math.max(1, Math.floor(span / cellSize));
  const key = (cx, cy, cz) => `${cx},${cy},${cz}`;
  const toCellIndex = (v, n) => Math.min(n - 1, Math.max(0, Math.floor((v + boxLimit) / cellSize)));

  const buckets = new Map();
  for (let i = 0; i < particles.length; i++) {
    const p = particles[i];
    const cx = toCellIndex(p.x, nx);
    const cy = toCellIndex(p.y, ny);
    const cz = toCellIndex(p.z, nz);
    const k = key(cx, cy, cz);
    if (!buckets.has(k)) buckets.set(k, []);
    buckets.get(k).push(i);
  }

  for (const [cellKey, listA] of buckets.entries()) {
    const [cx, cy, cz] = cellKey.split(",").map(Number);
    const idA = cx + cy * nx + cz * nx * ny;
    for (let dz = -1; dz <= 1; dz++) {
      for (let dy = -1; dy <= 1; dy++) {
        for (let dx = -1; dx <= 1; dx++) {
          const nxCell = cx + dx;
          const nyCell = cy + dy;
          const nzCell = cz + dz;
          if (nxCell < 0 || nyCell < 0 || nzCell < 0 || nxCell >= nx || nyCell >= ny || nzCell >= nz) continue;
          const listB = buckets.get(key(nxCell, nyCell, nzCell));
          if (!listB) continue;
          const idB = nxCell + nyCell * nx + nzCell * nx * ny;
          const sameCell = idA === idB;
          if (sameCell) {
            for (let ia = 0; ia < listA.length; ia++) {
              const i = listA[ia];
              const a = particles[i];
              for (let jb = ia + 1; jb < listA.length; jb++) {
                const j = listA[jb];
                const b = particles[j];
                ops += apply3DGravityPair(a, b, grav, soften);
              }
            }
          } else if (idB > idA) {
            for (let ia = 0; ia < listA.length; ia++) {
              const i = listA[ia];
              const a = particles[i];
              for (let jb = 0; jb < listB.length; jb++) {
                const j = listB[jb];
                const b = particles[j];
                ops += apply3DGravityPair(a, b, grav, soften);
              }
            }
          }
        }
      }
    }
  }
  return ops;
}

function apply3DGravityPair(a, b, grav, soften) {
  const dx = b.x - a.x;
  const dy = b.y - a.y;
  const dz = b.z - a.z;
  const dist2 = dx * dx + dy * dy + dz * dz + soften;
  const inv = 1 / Math.sqrt(dist2);
  const f = grav * inv * inv;
  const fx = f * dx * inv;
  const fy = f * dy * inv;
  const fz = f * dz * inv;
  a.ax += fx * b.m;
  a.ay += fy * b.m;
  a.az += fz * b.m;
  b.ax -= fx * a.m;
  b.ay -= fy * a.m;
  b.az -= fz * a.m;
  return 1;
}

function leapfrogKickDrift3D(particles, dt, timeScale, boxLimit, stable) {
  const dtScaled = dt * timeScale;
  const half = dtScaled * 0.5;
  const useBounds = !stable;
  for (const p of particles) {
    p.vx += p.ax * half;
    p.vy += p.ay * half;
    p.vz += p.az * half;
    p.x += p.vx * 60 * dtScaled;
    p.y += p.vy * 60 * dtScaled;
    p.z += p.vz * 60 * dtScaled;
    const r = p.r || 2;
    if (useBounds) {
      if (p.x < -boxLimit + r) { p.x = -boxLimit + r; p.vx *= -1; }
      if (p.x > boxLimit - r) { p.x = boxLimit - r; p.vx *= -1; }
      if (p.y < -boxLimit + r) { p.y = -boxLimit + r; p.vy *= -1; }
      if (p.y > boxLimit - r) { p.y = boxLimit - r; p.vy *= -1; }
      if (p.z < -boxLimit + r) { p.z = -boxLimit + r; p.vz *= -1; }
      if (p.z > boxLimit - r) { p.z = boxLimit - r; p.vz *= -1; }
    }
  }
}

function leapfrogKick3D(particles, dt, timeScale, energyLoss, damping, stable) {
  const dtScaled = dt * timeScale;
  const half = dtScaled * 0.5;
  const lossFactor = stable ? 1 : Math.max(0, 1 - energyLoss * dtScaled * 60);
  const dampingFactor = stable ? 1 : damping;
  for (const p of particles) {
    p.vx = (p.vx + p.ax * half) * dampingFactor * lossFactor;
    p.vy = (p.vy + p.ay * half) * dampingFactor * lossFactor;
    p.vz = (p.vz + p.az * half) * dampingFactor * lossFactor;
  }
}

function handleCollisionsAndMerge3D(particles, stable) {
  if (stable) return false;
  if (particles.length < 2) return false;

  let merged = false;
  const removed = new Set();

  forEachCollisionCandidatePair3D(particles, (i, j) => {
    if (removed.has(i) || removed.has(j)) return;
    const a = particles[i];
    const b = particles[j];
    if (!a || !b) return;

    const dx = b.x - a.x;
    const dy = b.y - a.y;
    const dz = b.z - a.z;
    const dist2 = dx * dx + dy * dy + dz * dz;
    const minDist = (a.r || 0) + (b.r || 0);
    if (dist2 <= minDist * minDist) {
      const mA = a.m;
      const mB = b.m;
      const mT = mA + mB;
      if (mT <= 0) return;
      const vx = (a.vx * mA + b.vx * mB) / mT;
      const vy = (a.vy * mA + b.vy * mB) / mT;
      const vz = (a.vz * mA + b.vz * mB) / mT;
      const x = (a.x * mA + b.x * mB) / mT;
      const y = (a.y * mA + b.y * mB) / mT;
      const z = (a.z * mA + b.z * mB) / mT;
      const rhoA = a.rho || 1;
      const rhoB = b.rho || 1;
      const rho = (rhoA * mA + rhoB * mB) / mT;
      const r = Math.cbrt((3 * mT) / (4 * Math.PI * rho));
      const color = mA > mB ? a.color : b.color;
      const bright = mA > mB ? a.bright || 1 : b.bright || 1;
      particles[i] = {
        x, y, z, vx, vy, vz, ax: 0, ay: 0, az: 0,
        r, rho, m: mT, color, bright,
      };
      removed.add(j);
      merged = true;
    }
  });

  if (merged) {
    let write = 0;
    for (let read = 0; read < particles.length; read++) {
      if (removed.has(read)) continue;
      particles[write] = particles[read];
      write++;
    }
    particles.length = write;
  }
  return merged;
}

function forEachCollisionCandidatePair3D(particles, callback) {
  const boxLimit = 380;
  const cellSize = getCollisionCellSize3D(particles);
  const span = boxLimit * 2;
  const nx = Math.max(1, Math.floor(span / cellSize));
  const ny = Math.max(1, Math.floor(span / cellSize));
  const nz = Math.max(1, Math.floor(span / cellSize));
  const key = (cx, cy, cz) => `${cx},${cy},${cz}`;
  const idOf = (cx, cy, cz) => cx + cy * nx + cz * nx * ny;
  const toCellIndex = (v, n) => Math.min(n - 1, Math.max(0, Math.floor((v + boxLimit) / cellSize)));

  const buckets = new Map();
  for (let i = 0; i < particles.length; i++) {
    const p = particles[i];
    const cx = toCellIndex(p.x, nx);
    const cy = toCellIndex(p.y, ny);
    const cz = toCellIndex(p.z, nz);
    const k = key(cx, cy, cz);
    let cell = buckets.get(k);
    if (!cell) {
      cell = { cx, cy, cz, indices: [] };
      buckets.set(k, cell);
    }
    cell.indices.push(i);
  }

  for (const cellA of buckets.values()) {
    const listA = cellA.indices;
    const idA = idOf(cellA.cx, cellA.cy, cellA.cz);
    for (let dz = -1; dz <= 1; dz++) {
      for (let dy = -1; dy <= 1; dy++) {
        for (let dx = -1; dx <= 1; dx++) {
          const nxCell = cellA.cx + dx;
          const nyCell = cellA.cy + dy;
          const nzCell = cellA.cz + dz;
          if (nxCell < 0 || nyCell < 0 || nzCell < 0 || nxCell >= nx || nyCell >= ny || nzCell >= nz) continue;

          const idB = idOf(nxCell, nyCell, nzCell);
          if (idB < idA) continue;

          const cellB = buckets.get(key(nxCell, nyCell, nzCell));
          if (!cellB) continue;
          const listB = cellB.indices;

          if (idB === idA) {
            for (let ia = 0; ia < listA.length; ia++) {
              for (let jb = ia + 1; jb < listA.length; jb++) {
                callback(listA[ia], listA[jb]);
              }
            }
          } else {
            for (let ia = 0; ia < listA.length; ia++) {
              for (let jb = 0; jb < listB.length; jb++) {
                callback(listA[ia], listB[jb]);
              }
            }
          }
        }
      }
    }
  }
}

function getCollisionCellSize3D(particles) {
  let maxRadius = 0;
  for (const p of particles) {
    maxRadius = Math.max(maxRadius, p.r || 0);
  }
  return Math.max(1, maxRadius * 2);
}
