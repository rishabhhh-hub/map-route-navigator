// ============================================================================
// Map Route Navigator — front-end visualizer
// Mirrors the algorithms implemented in cpp/astar.cpp on the same sample city.
// ============================================================================

// ---- Graph data (same sample city as the C++ demo) -----------------------

const NODES = [
  { id: 0, name: "A - City Center", x: 0,  y: 0 },
  { id: 1, name: "B - Market St",   x: 2,  y: 1 },
  { id: 2, name: "C - Riverside",   x: 1,  y: 3 },
  { id: 3, name: "D - Tech Park",   x: 5,  y: 2 },
  { id: 4, name: "E - Old Town",    x: -1, y: 2 },
  { id: 5, name: "F - Hilltop",     x: 3,  y: 5 },
  { id: 6, name: "G - Airport Rd",  x: 7,  y: 4 },
  { id: 7, name: "H - Lakeside",    x: 4,  y: -2 },
  { id: 8, name: "I - University",  x: 6,  y: 6 },
  { id: 9, name: "J - Stadium",     x: 8,  y: 1 },
];

const EDGES = [
  [0, 1, 2.2], [0, 2, 3.1], [0, 4, 2.0],
  [1, 3, 3.6], [1, 7, 3.8],
  [2, 5, 2.9], [2, 4, 2.4],
  [3, 6, 2.5], [3, 7, 3.3],
  [5, 8, 2.6], [5, 2, 2.9],
  [6, 9, 3.0],
  [8, 6, 2.2],
  [7, 9, 4.5],
  [4, 5, 4.0],
];

const adj = NODES.map(() => []);
for (const [u, v, w] of EDGES) {
  adj[u].push({ to: v, weight: w });
  adj[v].push({ to: u, weight: w });
}

function heuristic(a, b) {
  const dx = NODES[a].x - NODES[b].x;
  const dy = NODES[a].y - NODES[b].y;
  return Math.sqrt(dx * dx + dy * dy);
}

// ---- Minimal binary-heap priority queue -----------------------------------

class PriorityQueue {
  constructor() { this.items = []; }
  get size() { return this.items.length; }
  push(priority, value) {
    this.items.push([priority, value]);
    this.items.sort((a, b) => a[0] - b[0]); // small graph — simplicity over raw speed
  }
  pop() { return this.items.shift(); }
}

// ---- Search algorithms ------------------------------------------------

function runDijkstra(src, dest) {
  const t0 = performance.now();
  const n = NODES.length;
  const dist = Array(n).fill(Infinity);
  const parent = Array(n).fill(-1);
  const visited = Array(n).fill(false);
  const order = [];
  const pq = new PriorityQueue();

  dist[src] = 0;
  pq.push(0, src);

  while (pq.size) {
    const [d, u] = pq.pop();
    if (visited[u]) continue;
    visited[u] = true;
    order.push(u);
    if (u === dest) break;

    for (const e of adj[u]) {
      const nd = dist[u] + e.weight;
      if (nd < dist[e.to]) {
        dist[e.to] = nd;
        parent[e.to] = u;
        pq.push(nd, e.to);
      }
    }
  }

  const path = reconstruct(parent, dest);
  return {
    algo: "Dijkstra",
    path,
    cost: dist[dest] === Infinity ? -1 : dist[dest],
    order,
    exploredCount: order.length,
    timeMs: performance.now() - t0,
  };
}

function runAstar(src, dest) {
  const t0 = performance.now();
  const n = NODES.length;
  const gScore = Array(n).fill(Infinity);
  const parent = Array(n).fill(-1);
  const visited = Array(n).fill(false);
  const order = [];
  const pq = new PriorityQueue();

  gScore[src] = 0;
  pq.push(heuristic(src, dest), src);

  while (pq.size) {
    const [f, u] = pq.pop();
    if (visited[u]) continue;
    visited[u] = true;
    order.push(u);
    if (u === dest) break;

    for (const e of adj[u]) {
      const tentative = gScore[u] + e.weight;
      if (tentative < gScore[e.to]) {
        gScore[e.to] = tentative;
        parent[e.to] = u;
        pq.push(tentative + heuristic(e.to, dest), e.to);
      }
    }
  }

  const path = reconstruct(parent, dest);
  return {
    algo: "A*",
    path,
    cost: gScore[dest] === Infinity ? -1 : gScore[dest],
    order,
    exploredCount: order.length,
    timeMs: performance.now() - t0,
  };
}

function reconstruct(parent, dest) {
  if (parent[dest] === -1 && dest !== undefined) {
    // dest might legitimately have no parent only if it IS the source
  }
  const path = [];
  let at = dest;
  const guard = new Set();
  while (at !== -1 && at !== undefined && !guard.has(at)) {
    path.push(at);
    guard.add(at);
    at = parent[at];
  }
  path.reverse();
  return path;
}

// ---- Canvas setup -----------------------------------------------------

const canvas = document.getElementById("mapCanvas");
const ctx = canvas.getContext("2d");
const mapHint = document.getElementById("mapHint");

let source = null;
let dest = null;
let animating = false;

function resizeCanvas() {
  const rect = canvas.parentElement.getBoundingClientRect();
  const dpr = window.devicePixelRatio || 1;
  canvas.width = rect.width * dpr;
  canvas.height = rect.height * dpr;
  canvas.style.width = rect.width + "px";
  canvas.style.height = rect.height + "px";
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  draw();
}
window.addEventListener("resize", resizeCanvas);

function project(node) {
  const rect = canvas.getBoundingClientRect();
  const pad = 56;
  const xs = NODES.map(n => n.x), ys = NODES.map(n => n.y);
  const minX = Math.min(...xs), maxX = Math.max(...xs);
  const minY = Math.min(...ys), maxY = Math.max(...ys);
  const w = rect.width - pad * 2;
  const h = rect.height - pad * 2;
  const px = pad + ((node.x - minX) / (maxX - minX)) * w;
  const py = pad + (1 - (node.y - minY) / (maxY - minY)) * h;
  return { x: px, y: py };
}

let visitedSet = new Set();
let pathSet = new Set();
let pathEdgeSet = new Set();

function edgeKey(a, b) { return a < b ? `${a}-${b}` : `${b}-${a}`; }

function draw() {
  const rect = canvas.getBoundingClientRect();
  ctx.clearRect(0, 0, rect.width, rect.height);

  // roads
  ctx.lineWidth = 2;
  for (const [u, v] of EDGES) {
    const p1 = project(NODES[u]);
    const p2 = project(NODES[v]);
    const key = edgeKey(u, v);
    ctx.strokeStyle = pathEdgeSet.has(key)
      ? getVar("--path")
      : getVar("--road");
    ctx.lineWidth = pathEdgeSet.has(key) ? 4 : 2;
    ctx.beginPath();
    ctx.moveTo(p1.x, p1.y);
    ctx.lineTo(p2.x, p2.y);
    ctx.stroke();
  }

  // nodes
  for (const node of NODES) {
    const p = project(node);
    let fill = getVar("--node-fill");
    let stroke = getVar("--node-stroke");
    let radius = 8;

    if (visitedSet.has(node.id)) fill = getVar("--visited");
    if (pathSet.has(node.id)) { fill = getVar("--path"); radius = 9; }
    if (node.id === source) { fill = getVar("--source"); radius = 11; }
    if (node.id === dest) { fill = getVar("--dest"); radius = 11; }

    ctx.beginPath();
    ctx.arc(p.x, p.y, radius, 0, Math.PI * 2);
    ctx.fillStyle = fill;
    ctx.fill();
    ctx.lineWidth = 2;
    ctx.strokeStyle = stroke;
    ctx.stroke();

    ctx.font = "12px Inter, sans-serif";
    ctx.fillStyle = getVar("--text-secondary");
    ctx.textAlign = "center";
    const shortName = node.name.split(" - ")[0];
    ctx.fillText(shortName, p.x, p.y - radius - 8);
  }
}

function getVar(name) {
  return getComputedStyle(document.documentElement).getPropertyValue(name).trim();
}

canvas.addEventListener("click", (e) => {
  if (animating) return;
  const rect = canvas.getBoundingClientRect();
  const mx = e.clientX - rect.left;
  const my = e.clientY - rect.top;

  let closest = null, closestDist = Infinity;
  for (const node of NODES) {
    const p = project(node);
    const d = Math.hypot(p.x - mx, p.y - my);
    if (d < closestDist) { closestDist = d; closest = node; }
  }
  if (closestDist > 26) return; // missed all nodes

  selectNode(closest.id);
});

function selectNode(id) {
  if (source === null) {
    source = id;
  } else if (dest === null && id !== source) {
    dest = id;
  } else {
    source = id;
    dest = null;
  }
  clearResult();
  updateEndpointLabels();
  updateButtons();
  draw();
}

function updateEndpointLabels() {
  document.getElementById("sourceLabel").textContent =
    source !== null ? NODES[source].name.split(" - ")[0] : "—";
  document.getElementById("destLabel").textContent =
    dest !== null ? NODES[dest].name.split(" - ")[0] : "—";
  mapHint.style.display = (source !== null && dest !== null) ? "none" : "block";
}

function updateButtons() {
  const ready = source !== null && dest !== null;
  document.getElementById("runBtn").disabled = !ready;
  document.getElementById("compareBtn").disabled = !ready;
}

function clearResult() {
  visitedSet = new Set();
  pathSet = new Set();
  pathEdgeSet = new Set();
  document.getElementById("statsBlock").hidden = true;
}

// ---- Algorithm toggle ---------------------------------------------------

let currentAlgo = "astar";
const algoNotes = {
  astar: "Explores toward the destination using a straight-line distance estimate.",
  dijkstra: "Explores outward evenly in every direction by true road distance.",
};
document.querySelectorAll(".algo-btn").forEach(btn => {
  btn.addEventListener("click", () => {
    document.querySelectorAll(".algo-btn").forEach(b => b.classList.remove("active"));
    btn.classList.add("active");
    currentAlgo = btn.dataset.algo;
    document.getElementById("algoNote").textContent = algoNotes[currentAlgo];
  });
});

// ---- Run / animate --------------------------------------------------------

document.getElementById("runBtn").addEventListener("click", () => {
  if (source === null || dest === null || animating) return;
  const result = currentAlgo === "astar" ? runAstar(source, dest) : runDijkstra(source, dest);
  animateResult(result);
});

document.getElementById("compareBtn").addEventListener("click", () => {
  if (source === null || dest === null || animating) return;
  const a = runAstar(source, dest);
  const d = runDijkstra(source, dest);
  clearResult();
  pathSet = new Set(a.path);
  for (let i = 0; i < a.path.length - 1; i++) pathEdgeSet.add(edgeKey(a.path[i], a.path[i + 1]));
  draw();
  showCompareStats(a, d);
});

document.getElementById("resetBtn").addEventListener("click", () => {
  source = null;
  dest = null;
  animating = false;
  clearResult();
  updateEndpointLabels();
  updateButtons();
  draw();
});

function animateResult(result) {
  animating = true;
  visitedSet = new Set();
  pathSet = new Set();
  pathEdgeSet = new Set();
  document.getElementById("statsBlock").hidden = true;

  const speed = 420 - Number(document.getElementById("speedSlider").value); // slider: higher = faster
  let i = 0;

  function step() {
    if (i < result.order.length) {
      visitedSet.add(result.order[i]);
      draw();
      i++;
      setTimeout(step, speed);
    } else {
      pathSet = new Set(result.path);
      for (let k = 0; k < result.path.length - 1; k++) {
        pathEdgeSet.add(edgeKey(result.path[k], result.path[k + 1]));
      }
      draw();
      showSingleStats(result);
      animating = false;
    }
  }
  step();
}

// ---- Stats display ------------------------------------------------------

function statCard(label, value, wide) {
  return `<div class="stat-card${wide ? " wide" : ""}">
    <p class="stat-card-label">${label}</p>
    <p class="stat-card-value">${value}</p>
  </div>`;
}

function showSingleStats(result) {
  const grid = document.getElementById("statGrid");
  grid.innerHTML =
    statCard("Algorithm", result.algo) +
    statCard("Distance", result.cost.toFixed(1) + " km") +
    statCard("Nodes explored", `${result.exploredCount} / ${NODES.length}`) +
    statCard("Time", result.timeMs.toFixed(3) + " ms");
  document.getElementById("statsBlock").hidden = false;
}

function showCompareStats(a, d) {
  const grid = document.getElementById("statGrid");
  grid.innerHTML =
    statCard("A* nodes explored", `${a.exploredCount} / ${NODES.length}`) +
    statCard("Dijkstra nodes explored", `${d.exploredCount} / ${NODES.length}`) +
    statCard("A* time", a.timeMs.toFixed(3) + " ms") +
    statCard("Dijkstra time", d.timeMs.toFixed(3) + " ms") +
    statCard("Shortest distance (both agree)", a.cost.toFixed(1) + " km", true);
  document.getElementById("statsBlock").hidden = false;
}

// ---- Init -----------------------------------------------------------------

resizeCanvas();
updateEndpointLabels();
updateButtons();
