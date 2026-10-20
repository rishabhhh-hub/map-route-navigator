# Map Route Navigator

An interactive map-based route finder. A **C++17 backend** runs Dijkstra's algorithm and A\* on a weighted road graph and exposes them through a small REST API; a plain **HTML/CSS/JavaScript** page draws the map, sends requests, and animates the answers.

The browser never calculates a route. Every distance, path, explored-node list and timing on the page is returned by the C++ code.

---

## Contents

1. [Features](#features)
2. [Architecture](#architecture)
3. [What happens when you click "Find Shortest Route"](#request-flow)
4. [Data structures and algorithms](#data-structures-and-algorithms)
5. [Project layout](#project-layout)
6. [Prerequisites](#prerequisites)
7. [Build, run, test](#build-run-test)
8. [API reference](#api-reference)
9. [Troubleshooting](#troubleshooting)
10. [Verification status](#verification-status)
11. [Limitations and future work](#limitations-and-future-work)

---

## Features

- Sample city with 15 named intersections and 25 roads, defined **only** in the C++ backend (`GET /api/graph` delivers it to the page).
- Dijkstra's algorithm and A\* with a safe Euclidean heuristic, both on the same adjacency-list graph.
- Click nodes on the map (or use the drop-downs) to choose start and destination.
- Animated replay of the order in which the C++ search explored nodes, followed by the final path.
- "Compare Both Algorithms": sends two real requests and shows the two answers side by side.
- Input validation with JSON error messages; unreachable destinations, start = destination, malformed JSON and bad node ids are all handled.
- Server binds to `127.0.0.1` by default and serves exactly three frontend files.
- Respects `prefers-reduced-motion`, keyboard accessible nodes, light and dark colour schemes.

---

## Architecture

```
 Browser (HTML + CSS + JavaScript)                C++17 process: map_route_navigator
 ┌────────────────────────────────┐              ┌──────────────────────────────────────────┐
 │ index.html  style.css          │  GET /       │ ApiServer  (Api.cpp, cpp-httplib)        │
 │ script.js                      │◄────────────►│   serves index.html / style.css /        │
 │  - draws SVG map               │  static      │   script.js (fixed whitelist)            │
 │  - selectors, buttons          │              │                                          │
 │  - animation (setTimeout only  │  GET /api/health                                        │
 │    paces the visuals)          │  GET /api/graph        ┌─────────────────────────────┐  │
 │                                │◄──────────────────────►│ api::health / graphJson /   │  │
 │  fetch("/api/route", POST)     │  POST /api/route       │ route  (validate + JSON via │  │
 │  ───────────────────────────►  │  JSON in / JSON out    │ nlohmann/json)              │  │
 │                                │                        └──────────────┬──────────────┘  │
 └────────────────────────────────┘                                       │                 │
                                                          ┌───────────────▼──────────────┐  │
                                                          │ Algorithms.cpp               │  │
                                                          │  runDijkstra / runAStar      │  │
                                                          │  (min-priority queue, parent │  │
                                                          │   array, path reconstruction)│  │
                                                          └───────────────┬──────────────┘  │
                                                          ┌───────────────▼──────────────┐  │
                                                          │ Graph.cpp  adjacency list    │  │
                                                          │  nodes + weighted roads      │  │
                                                          │  createSampleCity()          │  │
                                                          └──────────────────────────────┘  │
                                                         └──────────────────────────────────┘
```

Because the C++ server also serves the web page, the page and the API share one origin (`http://127.0.0.1:8080`), so there are no cross-origin (CORS) problems.

<a id="request-flow"></a>
## What happens when you click "Find Shortest Route"

1. `onFindRoute()` in `frontend/script.js` checks that a start and destination are chosen.
2. `requestRoute()` calls `fetch("/api/route", { method: "POST", body: JSON.stringify({ start, destination, algorithm }) })`.
3. cpp-httplib (running inside `map_route_navigator`) matches the URL and calls `api::route(graph, body)` in `backend/Api.cpp`.
4. `api::route` parses the JSON without exceptions, checks the shape and types of every field, checks that both node ids exist, and checks the algorithm name. Any problem becomes a 400 response with a JSON message.
5. For a valid request it calls `runDijkstra(...)` or `runAStar(...)` in `backend/Algorithms.cpp`. The search settles nodes in priority order, records them in `exploredOrder`, stops when the destination is settled, rebuilds the path from the parent array, and measures its own run time with `std::chrono::steady_clock`.
6. The `RouteResult` is turned into JSON (`path`, `distance`, `exploredOrder`, `nodesExplored`, `executionTimeMs`, `reachable`, ...) and sent back (HTTP 200, or 404 with the same fields if the destination is unreachable).
7. Back in the browser, `requestRoute()` returns the parsed JSON. `showOnMap()` fills the results panel and `animateResult()` highlights the explored nodes in the order received, then the path.

The same flow is used twice, one request after the other, by "Compare Both Algorithms".

---

## Data structures and algorithms

| Concept | Where | Notes |
|---|---|---|
| Adjacency list | `Graph` (`std::vector<std::vector<Edge>>`) | Node ids equal vector indexes. An undirected road stores one `Edge` in each direction. |
| Road list | `Graph::roads()` | Roads as added, so the API reports each undirected road once. |
| Min-priority queue | `std::priority_queue` with a `greater`-style comparator | A max-heap flipped into a min-heap. Ties are broken by node id so results are deterministic. |
| Lazy deletion | `search()` in `Algorithms.cpp` | Instead of decrease-key, a better entry is pushed and stale ones are skipped when popped (`finalized[]`). |
| Distance / parent / finalized arrays | `search()` | `dist[v]` = best known g(v); `parent[v]` = predecessor; `finalized[v]` = settled. |
| Path reconstruction | `reconstructPath()` | Follow `parent` from the destination back to the start, then reverse. |

**Dijkstra** orders the queue by `g(n)`, the cost from the start. It needs non-negative weights, which `Graph::addRoad` enforces (negative, NaN and infinite weights are rejected).

**A\*** orders the queue by `f(n) = g(n) + h(n)`. Both algorithms share one implementation (`search()`); Dijkstra simply uses `h(n) = 0`, so the difference between them is only the heuristic.

**The heuristic.** Euclidean distance between coordinates is only a valid heuristic if no road is shorter than the straight line between its endpoints. The graph therefore computes `heuristicScale = min(1, min over roads of weight / straightLineLength)` and A\* uses `h(n) = heuristicScale × euclidean(n, destination)`. This is admissible (never overestimates) and consistent (`h(u) ≤ w(u,v) + h(v)`) for **any** weights. In the sample city every road is at least as long as its straight line, so the scale is exactly `1.0` (reported by `GET /api/graph` as `heuristicScale`). Because the heuristic is consistent, the first time A\* settles a node its cost is optimal, so the "settled" bookkeeping is correct.

**Complexity.** Both run in O((V + E) log V) time and O(V) extra space with a binary heap. A\* is not guaranteed to be faster than Dijkstra: with a weak heuristic it can explore as many nodes, and on tiny graphs the difference in time is dominated by noise.

**Explored order** means the order in which nodes were settled (first removed from the priority queue), including the destination as the last entry when a path exists.

---

## Project layout

```
map-route-navigator/
├── CMakeLists.txt            Build description (library + server + tests)
├── README.md                 This file
├── .gitignore
├── backend/
│   ├── main.cpp              Command-line options, starts the server
│   ├── Graph.h / Graph.cpp   Graph, adjacency list, sample city, heuristic scale
│   ├── Algorithms.h / .cpp   runDijkstra, runAStar, path reconstruction, timing
│   └── Api.h / Api.cpp       JSON request handling + HTTP server + static files
├── frontend/
│   ├── index.html            Page structure
│   ├── style.css             Responsive styling, light/dark, reduced motion
│   └── script.js             UI, fetch() calls, SVG drawing, animation
├── tests/
│   └── algorithm_tests.cpp   Self-contained tests (no framework needed)
└── third_party/              Vendored header-only libraries (see below)
    ├── httplib/httplib.h     cpp-httplib 0.18.3 (MIT)
    └── nlohmann/json.hpp     nlohmann/json 3.11.3 (MIT)
```

**Differences from the suggested structure** (all deliberate):

- `third_party/` was added. The two header-only libraries are included in the project, so you do not need to install them or have internet access to build. If you prefer to fetch them yourself, put the same two files in the same places:
  - https://github.com/yhirose/cpp-httplib/releases/tag/v0.18.3 (`httplib.h`)
  - https://github.com/nlohmann/json/releases/tag/v3.11.3 (`json.hpp`)
- `CMakeLists.txt` builds `Graph.cpp`, `Algorithms.cpp` and `Api.cpp` into one static library (`navigator_core`) that both the server and the tests link. Nothing is compiled twice.
- `Api.h` exposes pure functions (`api::route`, ...) next to the `ApiServer` class so the API logic can be unit-tested without opening sockets.

---

## Prerequisites

You need a C++17 compiler, CMake 3.16 or newer, and a build tool (Make or Ninja). No other dependencies.

### Windows 10/11 (MSYS2 + MinGW-w64, recommended)

1. Install MSYS2 from https://www.msys2.org/ (default location `C:\msys64`).
2. Open the **MSYS2 UCRT64** shell from the Start menu and run:

   ```bash
   pacman -Syu
   pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja
   ```

3. Add the toolchain to PATH for PowerShell. For the current session only:

   ```powershell
   $env:Path = "C:\msys64\ucrt64\bin;" + $env:Path
   g++ --version
   cmake --version
   ```

   (To make this permanent, add `C:\msys64\ucrt64\bin` to your user PATH in Windows Settings. The same folder must be on PATH when you *run* the program, because it holds the MinGW runtime DLLs.)

Use the UCRT64 (or MINGW64) toolchain. Avoid MinGW builds that use the "win32" thread model: cpp-httplib needs `std::thread`.

### Linux

```bash
# Debian / Ubuntu
sudo apt update && sudo apt install -y build-essential cmake
# Fedora
sudo dnf install -y gcc-c++ cmake make
```

### macOS

```bash
xcode-select --install     # Apple Clang
brew install cmake
```

---

## Build, run, test

All commands are run from the project root (the folder containing `CMakeLists.txt`).

### Windows PowerShell (MSYS2 UCRT64 toolchain on PATH)

```powershell
# Configure and build (Release)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Start the server
.\build\map_route_navigator.exe

# Run the tests (in a second PowerShell window, or after stopping the server)
ctest --test-dir build --output-on-failure
# or directly:
.\build\algorithm_tests.exe
```

If you used a Visual Studio generator instead of Ninja, the executables are in `build\Release\`.

### Linux / macOS

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

./build/map_route_navigator

ctest --test-dir build --output-on-failure
# or directly:
./build/algorithm_tests
```

### Open the website

Start the server, then open **http://127.0.0.1:8080** in a browser. The header shows "Backend online" once the page has loaded the graph from the C++ server.

Do **not** double-click `index.html`: the page needs the server for `/api/*` and for its own CSS and JavaScript.

### Server options

```
map_route_navigator [--host 127.0.0.1] [--port 8080] [--frontend <dir>]
```

- `--port 8081` uses another port if 8080 is busy.
- `--frontend <dir>` points at the folder with `index.html`, `style.css`, `script.js`. By default the server looks in `./frontend`, `../frontend` and `../../frontend`, so starting it from the project root or the `build` folder both work.
- `--host 0.0.0.0` exposes the server to your network. There is no authentication or HTTPS, so only do this on a network you trust. The default is local-only.

---

## API reference

All API bodies are JSON (`Content-Type: application/json; charset=utf-8`). Numbers below come from real runs of the sample city.

### `GET /api/health`

```json
{ "apiVersion": "1.0.0", "nodes": 15, "roads": 25, "status": "ok" }
```

### `GET /api/graph`

```json
{
  "nodes": [ { "id": 0, "name": "Central Station", "x": 450.0, "y": 280.0 }, "..." ],
  "edges": [ { "from": 0, "to": 1, "weight": 243.0, "bidirectional": true }, "..." ],
  "heuristicScale": 1.0
}
```

### `POST /api/route`

Request:

```json
{ "start": 3, "destination": 5, "algorithm": "astar" }
```

`algorithm` must be exactly `"dijkstra"` or `"astar"`. `start` and `destination` must be integer node ids.

Response (HTTP 200):

```json
{
  "algorithm": "astar",
  "start": 3,
  "destination": 5,
  "path": [3, 2, 1, 4, 5],
  "distance": 899.0,
  "exploredOrder": [3, 2, 1, 0, 4, 5],
  "nodesExplored": 6,
  "executionTimeMs": 0.003414,
  "reachable": true
}
```

The same request with `"dijkstra"` returns the same `path` and `distance` but `exploredOrder` `[3,2,10,11,12,1,0,9,6,4,7,5]` (12 nodes). `executionTimeMs` differs on every run.

Unreachable destination (HTTP **404**, the body still contains the full result):

```json
{
  "error": "no_route",
  "message": "No route exists between node 0 and node 14.",
  "algorithm": "astar", "start": 0, "destination": 14,
  "path": [], "distance": null, "reachable": false,
  "exploredOrder": [0, 6, 8, 9, 7, 4, 11, 1, 5, 2, 12, 10, 3],
  "nodesExplored": 13, "executionTimeMs": 0.004236
}
```

Start equal to destination returns 200 with `"path": [2]`, `"distance": 0.0`, `"exploredOrder": [2]`.

### Error responses

Every error has `{"error": "<code>", "message": "<text>"}`.

| HTTP | `error` | When |
|---|---|---|
| 400 | `malformed_json` | Body is empty or not valid JSON |
| 400 | `invalid_request` | JSON is valid but not an object |
| 400 | `missing_field` | `start`, `destination` or `algorithm` is missing |
| 400 | `invalid_field_type` | Wrong type (string/float/bool/null for a node id, non-string algorithm) |
| 400 | `invalid_node` | Node id is negative or not in the graph (valid ids are 0-14 for the sample city) |
| 400 | `unsupported_algorithm` | Algorithm is not `dijkstra` or `astar` |
| 404 | `no_route` | Both nodes exist but no road connects them (result fields included) |
| 404 | `http_error` | Unknown `/api/...` URL |
| 413 | `http_error` | Request body larger than 4096 bytes |
| 500 | `internal_error` | Unexpected server error |

### Trying the API from a terminal

Linux / macOS:

```bash
curl -s http://127.0.0.1:8080/api/health
curl -s -X POST http://127.0.0.1:8080/api/route \
     -H "Content-Type: application/json" \
     -d '{"start":3,"destination":5,"algorithm":"astar"}'
```

Windows PowerShell:

```powershell
Invoke-RestMethod http://127.0.0.1:8080/api/health

Invoke-RestMethod -Uri http://127.0.0.1:8080/api/route -Method Post `
  -ContentType "application/json" `
  -Body '{"start":3,"destination":5,"algorithm":"astar"}'
```

`Invoke-RestMethod` raises an error for non-2xx responses (for example the 404 for an unreachable destination). On PowerShell 7+ add `-SkipHttpErrorCheck` to see the body.

---

## Troubleshooting

| Symptom | Likely cause and fix |
|---|---|
| Page says "Backend offline" or "Cannot reach the C++ backend" | The server is not running, or you opened `index.html` as a file. Start `map_route_navigator` and use `http://127.0.0.1:8080`. |
| `Error: could not bind to 127.0.0.1:8080` | Another program uses the port. Use `--port 8081` and open that address. |
| Blank page or "Frontend file not found" | The server cannot see the `frontend` folder. Start it from the project root, or pass `--frontend <path>`. |
| Windows: program exits immediately / error `0xC0000135` | MinGW runtime DLLs not found. Add `C:\msys64\ucrt64\bin` to PATH (or run from the MSYS2 UCRT64 shell). |
| Windows: Firewall prompt on first start | The server listens on localhost only by default; you can allow or deny. |
| `cmake` errors about a different generator or compiler | Delete the `build` folder and configure again. |
| Compile error mentioning `std::thread` / `mutex` on Windows | You are using a MinGW toolchain with the win32 thread model. Install the UCRT64 packages above. |
| `CMake Error: ... requires at least version 3.16` | Update CMake. |
| Ctrl+C does not stop the server | Close the terminal window, or end `map_route_navigator` in Task Manager / `pkill map_route_navig`. |
| Route request returns 404 with `no_route` | Expected for nodes 13 and 14 (a separate island). Pick two nodes from 0-12. |

---

## Verification status

**Tested** (Ubuntu 24.04, g++ 13.3, CMake 3.28, built with `-Wall -Wextra -Wpedantic`, zero warnings from project code):

- **Unit tests:** 17 tests, 289,144 checks, all passing. They cover a known shortest path, different weights producing different paths, start = destination, unreachable nodes, one-way roads, Dijkstra/A\* agreement on every node pair of the sample city and on 300 random graphs (compared with an independent Floyd-Warshall implementation), heuristic admissibility and consistency, invalid node ids, graph input validation, and every API error code.
- **Live server:** all endpoints exercised with `curl` (health, graph, both algorithms, same node, unreachable, malformed JSON, missing fields, bad node id, bad algorithm, unknown URL, oversized body). Requests for `/../backend/main.cpp`, `/%2e%2e/...`, `/backend/main.cpp` and `/etc/passwd` all return 404.
- **Frontend against the live server:** the page's real `script.js` was run in jsdom (a simulated browser, no rendering) against the running C++ server. It checked that the graph is drawn from `/api/graph`, that Find and Compare send the expected `POST /api/route` bodies, that the results panel shows the backend's numbers, that the animation shows explored nodes before the path, and that unreachable, start = destination, reset, node-click selection and "backend offline" behave correctly. That script is a throw-away verification aid and is not part of this project.
- **Static check:** `script.js` contains no pathfinding code and no copy of the graph; its only `fetch` calls are the `/api/*` requests.

**Not tested:**

- Rendering in a real browser: visual layout, colours, SVG label overlap, mobile screen sizes, the dark theme, and the Content-Security-Policy header interacting with a real browser.
- Windows / MinGW-w64, macOS, Clang and MSVC builds. The Windows instructions and the CMake Windows settings (`ws2_32`, `_WIN32_WINNT`) follow cpp-httplib's documented requirements but have not been run.
- Behaviour on graphs much larger than the sample, concurrency under load, and IPv6 binding.

---

## Limitations and future work

- The graph is hard-coded in `Graph::createSampleCity()`. Loading it from a file (JSON/CSV) or from OpenStreetMap data would be the natural next step.
- Edge weights are static distances. Real navigation would add speed limits, traffic, turn costs and one-way streets in the data.
- Timings come from one run on a 15-node graph and are noisy; a fair benchmark needs larger graphs, many repetitions and statistics.
- Straight-line coordinates are treated as flat 2-D map units. For latitude/longitude you would use haversine distance for the heuristic.
- No authentication, HTTPS or rate limiting; the server is meant for local development.
- Possible additions: bidirectional search, ALT/landmark heuristics, editing roads in the UI, multiple stops, and a step-by-step "pause/resume" visualiser.

## License and third-party code

The project code is yours to use as you like. `third_party/httplib/httplib.h` (cpp-httplib, Yuji Hirose) and `third_party/nlohmann/json.hpp` (nlohmann/json, Niels Lohmann) are distributed under the MIT License; keep their license headers when redistributing.
