#include "Algorithms.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <functional>
#include <queue>
#include <stdexcept>

namespace nav {

namespace {

constexpr double kInfinity = std::numeric_limits<double>::infinity();

// An entry of the min-priority queue.
//   Dijkstra: priority = g(n)            (cost from the start)
//   A*      : priority = f(n) = g + h    (cost so far + estimated cost left)
struct QueueEntry {
    double priority;
    int node;
};

// std::priority_queue is a max-heap, so "greater" turns it into a min-heap.
// Ties are broken by node id to keep the exploration order deterministic.
struct QueueEntryGreater {
    bool operator()(const QueueEntry& a, const QueueEntry& b) const {
        if (a.priority != b.priority) return a.priority > b.priority;
        return a.node > b.node;
    }
};

using MinQueue =
    std::priority_queue<QueueEntry, std::vector<QueueEntry>, QueueEntryGreater>;

void checkNode(const Graph& graph, int id, const char* what) {
    if (!graph.hasNode(id)) {
        throw std::out_of_range(std::string("unknown ") + what + " node id");
    }
}

// Walks the parent links backwards from `destination` to `start`.
std::vector<int> reconstructPath(const std::vector<int>& parent, int start,
                                 int destination) {
    std::vector<int> path;
    for (int at = destination; at != -1; at = parent[static_cast<std::size_t>(at)]) {
        path.push_back(at);
        if (at == start) break;
    }
    std::reverse(path.begin(), path.end());
    return path;
}

double elapsedMs(std::chrono::steady_clock::time_point begin) {
    return std::chrono::duration<double, std::milli>(
               std::chrono::steady_clock::now() - begin)
        .count();
}

// Shared implementation. `heuristic` returns h(n); Dijkstra passes a function
// that always returns 0, which turns f(n) = g(n) + h(n) into plain g(n).
//
// Both searches use "lazy deletion": instead of decreasing a key inside the
// heap, a better entry is pushed and stale entries are skipped when popped.
RouteResult search(const Graph& graph, int start, int destination,
                   const std::string& algorithmName,
                   const std::function<double(int)>& heuristic) {
    checkNode(graph, start, "start");
    checkNode(graph, destination, "destination");

    const auto begin = std::chrono::steady_clock::now();

    const std::size_t n = graph.nodeCount();
    std::vector<double> dist(n, kInfinity);  // best known g(n)
    std::vector<int> parent(n, -1);          // predecessor on the best path
    std::vector<bool> finalized(n, false);   // node has been settled

    RouteResult result;
    result.algorithm = algorithmName;
    result.start = start;
    result.destination = destination;

    MinQueue open;
    dist[static_cast<std::size_t>(start)] = 0.0;
    open.push(QueueEntry{heuristic(start), start});

    while (!open.empty()) {
        const QueueEntry current = open.top();
        open.pop();
        const int u = current.node;
        const std::size_t ui = static_cast<std::size_t>(u);

        if (finalized[ui]) continue;  // stale queue entry
        finalized[ui] = true;
        result.exploredOrder.push_back(u);

        if (u == destination) break;  // shortest path to the goal is final

        for (const Edge& edge : graph.neighbors(u)) {
            const std::size_t vi = static_cast<std::size_t>(edge.to);
            if (finalized[vi]) continue;
            const double candidate = dist[ui] + edge.weight;
            if (candidate < dist[vi]) {
                dist[vi] = candidate;
                parent[vi] = u;
                open.push(QueueEntry{candidate + heuristic(edge.to), edge.to});
            }
        }
    }

    const std::size_t di = static_cast<std::size_t>(destination);
    if (finalized[di]) {
        result.reachable = true;
        result.distance = dist[di];
        result.path = reconstructPath(parent, start, destination);
    }
    result.executionTimeMs = elapsedMs(begin);
    return result;
}

}  // namespace

RouteResult runDijkstra(const Graph& graph, int start, int destination) {
    return search(graph, start, destination, "dijkstra",
                  [](int) { return 0.0; });
}

RouteResult runAStar(const Graph& graph, int start, int destination) {
    // Validate first so heuristic() is never asked about an unknown node.
    checkNode(graph, start, "start");
    checkNode(graph, destination, "destination");
    return search(graph, start, destination, "astar",
                  [&graph, destination](int node) {
                      return graph.heuristic(node, destination);
                  });
}

}  // namespace nav
