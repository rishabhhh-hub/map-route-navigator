# route/nav — Map Route Navigator (DSA project)

A shortest-path route navigator, like a small-scale Google Maps. It models a
city as a weighted graph and finds the optimal route between two
intersections using **A\* search**, compared against **Dijkstra's
algorithm**.

Two parts, sharing the same sample city graph so their results line up:

- `cpp/astar.cpp` — the core algorithm in C++: builds the graph, runs both
  Dijkstra and A*, and prints a side-by-side comparison to the console.
- `web/` — a browser-based visualizer (plain HTML/CSS/JS, no build step)
  where you click two intersections and watch each algorithm explore the
  map node by node. Deploy it directly with GitHub Pages.

## Problem statement

Given a set of locations (nodes) connected by roads (edges, weighted by
distance), find the shortest path between a source and a destination —
the same problem every turn-by-turn navigation app solves.

## Why two algorithms

**Dijkstra's algorithm** finds the shortest path by expanding outward from
the source in every direction, always visiting the closest unvisited node
next. It's guaranteed correct and has no notion of where the destination
is.

**A\* search** does the same expansion, but ranks each candidate node by
`f(n) = g(n) + h(n)`:
- `g(n)` — the real distance traveled so far
- `h(n)` — a heuristic estimate of the remaining distance to the goal
  (here, straight-line/Euclidean distance)

Because the heuristic never overestimates the true remaining road distance
(a straight line is never longer than the road), A* is **admissible** — it
still guarantees the shortest path — but it explores far fewer nodes,
because it's biased toward the destination instead of spreading out evenly.
This is the property that makes it the standard choice for real-world map
navigation.

On the sample 10-node city in this repo, Dijkstra visits all 10 nodes to
confirm the answer; A* reaches the same shortest path after visiting only 6.

## Project structure

```
mapnav/
├── cpp/
│   └── astar.cpp        # Graph, Dijkstra, A*, console comparison demo
├── web/
│   ├── index.html        # Visualizer layout
│   ├── style.css         # Dark map theme
│   └── script.js         # Graph data + algorithms (mirrors the C++) + canvas animation
├── .gitignore
└── README.md
```

## Running the C++ demo

```bash
cd cpp
g++ -std=c++17 -O2 -o navigator astar.cpp
./navigator
```

Prints the path, total distance, nodes explored, and timing for both
algorithms on the sample city.

## Running the web visualizer

No build step — just open it, or serve it locally:

```bash
cd web
python3 -m http.server 8000
# then open http://localhost:8000
```

Click any intersection to set the start, click another to set the
destination, then **Find route** to watch the search animate, or
**Compare both** to see Dijkstra and A* stats side by side instantly.

## Deploying to GitHub Pages

1. Push this repo to GitHub.
2. In the repo settings, under **Pages**, set the source to the `web/`
   folder on your main branch (or copy `web/`'s contents to the repo root
   if your Pages setup expects `index.html` at the root).
3. Your visualizer will be live at `https://<username>.github.io/<repo>/`.

## Presenting this to your teacher

Good talking points, in order:
1. **Problem** — shortest path between two points on a weighted graph.
2. **Baseline** — Dijkstra, why it's correct, why it explores more than it
   needs to.
3. **Improvement** — A*, the `f = g + h` formula, why the heuristic keeps it
   correct (admissibility) while cutting down exploration.
4. **Live demo** — run the same start/destination in the visualizer with
   both algorithms and point at the difference in nodes explored.
5. **Complexity** — both are `O((V + E) log V)` with a binary heap; A*'s
   advantage is practical (fewer nodes touched), not in the big-O bound.

## Possible extensions

- Weight roads by live "traffic" (randomized multipliers) and re-route.
- Add a toggle for "shortest distance" vs "fastest time" as different edge
  weights.
- Swap the hardcoded city for a real dataset (e.g. exported OpenStreetMap
  nodes for a neighborhood).
