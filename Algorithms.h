// Algorithms.h - Dijkstra and A* shortest-path searches.
#pragma once

#include <limits>
#include <string>
#include <vector>

#include "Graph.h"

namespace nav {

struct RouteResult {
    std::string algorithm;  // "dijkstra" or "astar"
    int start = -1;
    int destination = -1;
    bool reachable = false;

    // Node ids from start to destination (inclusive). Empty if unreachable.
    std::vector<int> path;

    // Total weight of `path`. +infinity if unreachable.
    double distance = std::numeric_limits<double>::infinity();

    // Nodes in the order the search finalised ("settled") them, i.e. the order
    // in which they were taken off the priority queue for the first time.
    std::vector<int> exploredOrder;

    // Wall-clock time spent inside the algorithm, in milliseconds.
    double executionTimeMs = 0.0;
};

// Both functions throw std::out_of_range if `start` or `destination` is not a
// node of `graph`. Edge weights are guaranteed non-negative by Graph::addRoad.
RouteResult runDijkstra(const Graph& graph, int start, int destination);
RouteResult runAStar(const Graph& graph, int start, int destination);

}  // namespace nav
