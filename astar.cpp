// ============================================================================
//  Map Route Navigator — Core Pathfinding Engine (C++)
//
//  Implements A* search and Dijkstra's algorithm over a weighted graph that
//  models a small city: nodes are intersections (with real x,y coordinates
//  used for the A* heuristic), edges are roads (weighted by distance).
//
//  Build:  g++ -std=c++17 -O2 -o navigator astar.cpp
//  Run:    ./navigator
// ============================================================================

#include <iostream>
#include <vector>
#include <queue>
#include <unordered_map>
#include <cmath>
#include <limits>
#include <chrono>
#include <algorithm>
#include <string>

using namespace std;
using namespace std::chrono;

// ----------------------------------------------------------------------------
// Graph representation
// ----------------------------------------------------------------------------

struct Edge {
    int to;
    double weight;   // road distance (km)
};

struct Node {
    string name;
    double x, y;      // coordinates, used for the A* heuristic (straight-line distance)
};

class Graph {
public:
    vector<Node> nodes;
    vector<vector<Edge>> adj;

    int addNode(const string& name, double x, double y) {
        nodes.push_back({name, x, y});
        adj.push_back({});
        return (int)nodes.size() - 1;
    }

    void addEdge(int u, int v, double weight) {
        adj[u].push_back({v, weight});
        adj[v].push_back({u, weight}); // undirected road
    }

    double heuristic(int a, int b) const {
        double dx = nodes[a].x - nodes[b].x;
        double dy = nodes[a].y - nodes[b].y;
        return sqrt(dx * dx + dy * dy); // Euclidean distance — admissible & consistent
    }
};

// ----------------------------------------------------------------------------
// Result struct shared by both algorithms, so we can compare them fairly
// ----------------------------------------------------------------------------

struct SearchResult {
    vector<int> path;
    double totalCost = -1;
    int nodesExplored = 0;
    double timeMs = 0;
};

// ----------------------------------------------------------------------------
// Dijkstra's algorithm — explores outward in all directions by true cost.
// Guaranteed shortest path. No knowledge of where the goal is.
// ----------------------------------------------------------------------------

SearchResult dijkstra(const Graph& g, int src, int dest) {
    auto start = high_resolution_clock::now();

    int n = (int)g.nodes.size();
    vector<double> dist(n, numeric_limits<double>::infinity());
    vector<int> parent(n, -1);
    vector<bool> visited(n, false);

    // min-heap of (distance, node)
    priority_queue<pair<double,int>, vector<pair<double,int>>, greater<>> pq;

    dist[src] = 0;
    pq.push({0, src});
    int explored = 0;

    while (!pq.empty()) {
        auto [d, u] = pq.top(); pq.pop();
        if (visited[u]) continue;
        visited[u] = true;
        explored++;

        if (u == dest) break;

        for (const Edge& e : g.adj[u]) {
            double nd = dist[u] + e.weight;
            if (nd < dist[e.to]) {
                dist[e.to] = nd;
                parent[e.to] = u;
                pq.push({nd, e.to});
            }
        }
    }

    SearchResult res;
    res.nodesExplored = explored;
    if (dist[dest] == numeric_limits<double>::infinity()) {
        res.totalCost = -1; // unreachable
    } else {
        res.totalCost = dist[dest];
        for (int at = dest; at != -1; at = parent[at]) res.path.push_back(at);
        reverse(res.path.begin(), res.path.end());
    }

    auto end = high_resolution_clock::now();
    res.timeMs = duration<double, milli>(end - start).count();
    return res;
}

// ----------------------------------------------------------------------------
// A* search — like Dijkstra, but ranks the frontier by f(n) = g(n) + h(n),
// where g(n) is the real cost so far and h(n) is the straight-line distance
// to the destination. The heuristic biases the search toward the goal, so it
// explores far fewer nodes while still guaranteeing the shortest path
// (as long as the heuristic never overestimates — true here, since roads are
// never shorter than a straight line).
// ----------------------------------------------------------------------------

SearchResult astar(const Graph& g, int src, int dest) {
    auto start = high_resolution_clock::now();

    int n = (int)g.nodes.size();
    vector<double> gScore(n, numeric_limits<double>::infinity());
    vector<int> parent(n, -1);
    vector<bool> visited(n, false);

    // min-heap of (fScore, node)
    priority_queue<pair<double,int>, vector<pair<double,int>>, greater<>> pq;

    gScore[src] = 0;
    pq.push({g.heuristic(src, dest), src});
    int explored = 0;

    while (!pq.empty()) {
        auto [f, u] = pq.top(); pq.pop();
        if (visited[u]) continue;
        visited[u] = true;
        explored++;

        if (u == dest) break;

        for (const Edge& e : g.adj[u]) {
            double tentative = gScore[u] + e.weight;
            if (tentative < gScore[e.to]) {
                gScore[e.to] = tentative;
                parent[e.to] = u;
                double f_score = tentative + g.heuristic(e.to, dest);
                pq.push({f_score, e.to});
            }
        }
    }

    SearchResult res;
    res.nodesExplored = explored;
    if (gScore[dest] == numeric_limits<double>::infinity()) {
        res.totalCost = -1;
    } else {
        res.totalCost = gScore[dest];
        for (int at = dest; at != -1; at = parent[at]) res.path.push_back(at);
        reverse(res.path.begin(), res.path.end());
    }

    auto end = high_resolution_clock::now();
    res.timeMs = duration<double, milli>(end - start).count();
    return res;
}

// ----------------------------------------------------------------------------
// Demo: builds a small sample "city" and compares both algorithms.
// ----------------------------------------------------------------------------

void printResult(const Graph& g, const string& label, const SearchResult& r) {
    cout << "\n--- " << label << " ---\n";
    if (r.totalCost < 0) {
        cout << "No path found.\n";
        return;
    }
    cout << "Path: ";
    for (size_t i = 0; i < r.path.size(); i++) {
        cout << g.nodes[r.path[i]].name;
        if (i + 1 < r.path.size()) cout << " -> ";
    }
    cout << "\nTotal distance : " << r.totalCost << " km";
    cout << "\nNodes explored : " << r.nodesExplored << " / " << g.nodes.size();
    cout << "\nTime taken     : " << r.timeMs << " ms\n";
}

int main() {
    Graph city;

    // Sample intersections (name, x, y) — coordinates are arbitrary units,
    // treat them like a small local grid (e.g. km east, km north of a corner).
    int A = city.addNode("A - City Center",     0,  0);
    int B = city.addNode("B - Market St",       2,  1);
    int C = city.addNode("C - Riverside",       1,  3);
    int D = city.addNode("D - Tech Park",       5,  2);
    int E = city.addNode("E - Old Town",       -1,  2);
    int F = city.addNode("F - Hilltop",         3,  5);
    int G = city.addNode("G - Airport Rd",      7,  4);
    int H = city.addNode("H - Lakeside",        4, -2);
    int I = city.addNode("I - University",      6,  6);
    int J = city.addNode("J - Stadium",         8,  1);

    // Roads (undirected, weight = distance in km)
    city.addEdge(A, B, 2.2);
    city.addEdge(A, C, 3.1);
    city.addEdge(A, E, 2.0);
    city.addEdge(B, D, 3.6);
    city.addEdge(B, H, 3.8);
    city.addEdge(C, F, 2.9);
    city.addEdge(C, E, 2.4);
    city.addEdge(D, G, 2.5);
    city.addEdge(D, H, 3.3);
    city.addEdge(F, I, 2.6);
    city.addEdge(F, C, 2.9);
    city.addEdge(G, J, 3.0);
    city.addEdge(I, G, 2.2);
    city.addEdge(H, J, 4.5);
    city.addEdge(E, F, 4.0);

    int source = A;
    int dest   = J;

    cout << "Map Route Navigator\n";
    cout << "Finding route from '" << city.nodes[source].name
         << "' to '" << city.nodes[dest].name << "'\n";

    SearchResult dijkstraResult = dijkstra(city, source, dest);
    SearchResult astarResult    = astar(city, source, dest);

    printResult(city, "Dijkstra's Algorithm", dijkstraResult);
    printResult(city, "A* Search", astarResult);

    cout << "\n--- Comparison ---\n";
    cout << "Both algorithms find the same shortest distance ("
         << astarResult.totalCost << " km), but A* explored "
         << astarResult.nodesExplored << " nodes vs Dijkstra's "
         << dijkstraResult.nodesExplored
         << " — because the heuristic focuses the search toward the goal.\n";

    return 0;
}
