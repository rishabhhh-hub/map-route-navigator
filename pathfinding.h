// pathfinding.h - the graph and both algorithms (Dijkstra and A*).
// This is the DSA part of the project. Everything is in this one header.

#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <limits>
#include <queue>
#include <string>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// Graph (adjacency list)
// ---------------------------------------------------------------------------

struct Node {
    int id;
    std::string name;
    double x, y;  // position on the map
};

struct Edge {
    int to;        // the node this road leads to
    double weight; // length of the road
};

struct Road {      // used only to send the roads to the web page
    int from, to;
    double weight;
};

struct Graph {
    std::vector<Node> nodes;
    std::vector<std::vector<Edge>> adj;  // adj[u] = list of roads leaving u
    std::vector<Road> roads;

    int addNode(const std::string& name, double x, double y) {
        int id = (int)nodes.size();
        nodes.push_back({id, name, x, y});
        adj.push_back({});
        return id;
    }

    // Roads go both ways, so we store the edge in both directions.
    void addRoad(int a, int b, double weight) {
        adj[a].push_back({b, weight});
        adj[b].push_back({a, weight});
        roads.push_back({a, b, weight});
    }

    bool validNode(int id) const { return id >= 0 && id < (int)nodes.size(); }
};

// A small sample city: 15 places, 25 roads.
// Nodes 13 and 14 are only connected to each other (an "island"),
// so you can try an unreachable route.
inline Graph makeSampleCity() {
    Graph g;
    g.addNode("Central Station", 450, 280);  // 0
    g.addNode("North Gate", 430, 60);        // 1
    g.addNode("Market Square", 260, 170);    // 2
    g.addNode("Old Town", 90, 250);          // 3
    g.addNode("University", 640, 140);       // 4
    g.addNode("City Hospital", 810, 80);     // 5
    g.addNode("Riverside Park", 620, 330);   // 6
    g.addNode("Tech Park", 820, 290);        // 7
    g.addNode("Harbor", 760, 480);           // 8
    g.addNode("Stadium", 480, 490);          // 9
    g.addNode("West End", 110, 470);         // 10
    g.addNode("South Mall", 290, 400);       // 11
    g.addNode("Lakeview", 250, 40);          // 12
    g.addNode("Ferry Pier", 870, 380);       // 13
    g.addNode("Island Depot", 880, 530);     // 14

    // Every road is at least as long as the straight line between its ends.
    // That is what makes the straight-line heuristic safe for A*.
    int r[][3] = {
        {0, 1, 243},  {0, 2, 253},  {0, 6, 187},  {0, 9, 255},  {0, 11, 250},
        {0, 4, 307},  {1, 2, 223},  {1, 4, 270},  {1, 12, 209}, {2, 12, 157},
        {2, 3, 207},  {2, 11, 302}, {3, 10, 266}, {3, 11, 350}, {10, 11, 223},
        {11, 9, 253}, {4, 5, 199},  {4, 6, 220},  {5, 7, 263},  {6, 7, 225},
        {6, 8, 267},  {7, 8, 240},  {8, 9, 309},  {6, 9, 319},  {13, 14, 166},
    };
    for (auto& road : r) g.addRoad(road[0], road[1], road[2]);
    return g;
}

// ---------------------------------------------------------------------------
// Dijkstra and A*
// ---------------------------------------------------------------------------

struct Result {
    bool reachable = false;
    std::vector<int> path;      // node ids from start to destination
    double distance = 0;        // total length of the path
    std::vector<int> explored;  // nodes in the order the search finished them
    double timeMs = 0;          // how long the search took
};

// One function runs both algorithms.
//   Dijkstra: queue is ordered by  g(n)         (distance travelled so far)
//   A*      : queue is ordered by  g(n) + h(n)  (plus a guess of the distance left)
// Dijkstra is simply A* with the guess h(n) = 0.
//
// Both functions assume start and destination are valid node ids.
inline Result findRoute(const Graph& g, int start, int destination, bool useAStar) {
    auto t0 = std::chrono::steady_clock::now();

    const double INF = std::numeric_limits<double>::infinity();
    int n = (int)g.nodes.size();

    std::vector<double> dist(n, INF);  // best distance found so far to each node
    std::vector<int> parent(n, -1);    // the node we came from (for the path)
    std::vector<bool> done(n, false);  // is the best distance final?

    // h(n): straight-line distance to the destination (0 for Dijkstra)
    auto h = [&](int u) {
        if (!useAStar) return 0.0;
        return std::hypot(g.nodes[u].x - g.nodes[destination].x,
                          g.nodes[u].y - g.nodes[destination].y);
    };

    // Min-priority queue of (priority, node). The smallest priority comes out first.
    using Entry = std::pair<double, int>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> queue;

    Result result;
    dist[start] = 0;
    queue.push({h(start), start});

    while (!queue.empty()) {
        int u = queue.top().second;
        queue.pop();

        if (done[u]) continue;  // old queue entry, we already handled this node
        done[u] = true;
        result.explored.push_back(u);

        if (u == destination) break;  // shortest path found

        for (const Edge& e : g.adj[u]) {
            double newDist = dist[u] + e.weight;
            if (newDist < dist[e.to]) {  // found a shorter way to e.to
                dist[e.to] = newDist;
                parent[e.to] = u;
                queue.push({newDist + h(e.to), e.to});
            }
        }
    }

    if (done[destination]) {
        result.reachable = true;
        result.distance = dist[destination];
        // Build the path by walking backwards using parent, then reverse it.
        for (int at = destination; at != -1; at = parent[at]) result.path.push_back(at);
        std::reverse(result.path.begin(), result.path.end());
    }

    result.timeMs = std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - t0).count();
    return result;
}
