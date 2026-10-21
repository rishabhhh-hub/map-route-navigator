# Map Route Navigator (simple version)

A small web app that finds the shortest route between two places on a map.
The route is calculated by a **C++ program** using **Dijkstra's algorithm** and **A\***.
The web page only sends the request and draws the answer.

## How it works

```
Browser (HTML + CSS + JavaScript)  <-- JSON over HTTP -->  C++ server
  draws the map, sends requests                            holds the map, runs Dijkstra / A*
```

1. The page asks `GET /api/graph` and draws the map.
2. You pick a start and a destination and press a button.
3. The page sends `POST /api/route` with `{"start": 3, "destination": 5, "algorithm": "astar"}`.
4. The C++ server runs the algorithm and replies with the path, distance, explored nodes and time.
5. The page draws the explored nodes, then the final path.

## Files

| File | What it does |
|---|---|
| `backend/pathfinding.h` | The graph (adjacency list), the sample city, Dijkstra and A*. **The main DSA file.** |
| `backend/main.cpp` | The web server and the two API endpoints |
| `frontend/index.html` | Page structure |
| `frontend/style.css` | Page styling |
| `frontend/script.js` | Draws the map, calls the server with `fetch()`, shows results |
| `tests/tests.cpp` | Simple tests (prints PASS / FAIL) |
| `CMakeLists.txt` | Build instructions |
| `third_party/` | Two helper libraries (`httplib.h` web server, `json.hpp` JSON). Do not edit. |

## Data structures and algorithms

- **Graph**: adjacency list (`vector<vector<Edge>>`). Each road is stored in both directions.
- **Min-priority queue**: `std::priority_queue` with `std::greater`, so the smallest value comes out first.
- **Dijkstra**: always processes the node with the smallest distance travelled so far.
- **A\***: same, but orders by `distance so far + straight-line distance to the goal`. That second part is the *heuristic*.
  Dijkstra is just A* with the heuristic set to 0.
- **Path**: every node remembers its `parent`. We walk back from the destination and reverse the list.
- **Time complexity**: O((V + E) log V).
- A* is **not** always faster than Dijkstra. It often explores fewer nodes, but on a map this small the time differences are noise.

## Run it on Windows 11

One-time setup: install MSYS2 from https://www.msys2.org, open **MSYS2 UCRT64** and run:

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja
```

Then open PowerShell in the project folder:

```powershell
$env:Path = "C:\msys64\ucrt64\bin;" + $env:Path
cmake -S . -B build -G Ninja
cmake --build build
.\build\server.exe
```

Open **http://127.0.0.1:8080** in your browser. Press Ctrl+C in PowerShell to stop.
Next time you only need the `$env:Path` line and `.\build\server.exe`.

To run the tests: `.\build\tests.exe`

## Run it on Linux / macOS

```bash
cmake -S . -B build
cmake --build build
./build/server        # then open http://127.0.0.1:8080
./build/tests
```

## API

`GET /api/graph` returns `{"nodes": [...], "roads": [...]}`.

`POST /api/route` with `{"start": 3, "destination": 5, "algorithm": "dijkstra"}` (or `"astar"`) returns:

```json
{"algorithm": "astar", "reachable": true, "path": [3, 2, 1, 4, 5], "distance": 899.0,
 "exploredOrder": [3, 2, 1, 0, 4, 5], "nodesExplored": 6, "executionTimeMs": 0.008}
```

Bad input (broken JSON, missing field, unknown node, unknown algorithm) returns HTTP 400 with `{"error": "..."}`.
If there is no road connection, `reachable` is `false` and `path` is empty.

## Troubleshooting

- **Page says "Cannot reach the C++ server"**: start `server` first and open `http://127.0.0.1:8080` (do not double-click `index.html`).
- **"Could not start"**: port 8080 is busy. Close the other program.
- **Windows: program closes at once or shows a DLL error**: run the `$env:Path = ...` line first.
- **Try an unreachable route**: pick node 13 or 14 and any other node.

## Limitations

The map is fixed in the code, roads have fixed lengths, and the server is for local use only (no login or HTTPS).
