// Graph.h - weighted road network stored as an adjacency list.
//
// Intersections are nodes (id, name, x/y coordinates). Roads are weighted
// edges. Roads are undirected by default: addRoad() stores one edge in each
// direction so the algorithms only ever have to look at "outgoing" edges.
#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace nav {

// An intersection. `id` is always equal to the node's index in the graph.
struct Node {
    int id;
    std::string name;
    double x;
    double y;
};

// One outgoing edge in the adjacency list.
struct Edge {
    int to;
    double weight;
};

// A road exactly as it was added (used to describe the graph through the API,
// so that an undirected road is reported once instead of twice).
struct Road {
    int from;
    int to;
    double weight;
    bool bidirectional;
};

class Graph {
public:
    // Adds an intersection and returns its id (0, 1, 2, ...).
    // Throws std::invalid_argument if a coordinate is NaN or infinite.
    int addNode(const std::string& name, double x, double y);

    // Adds a road between two existing nodes.
    // Throws std::out_of_range      if either id is unknown,
    //        std::invalid_argument  if weight is negative / NaN / infinite,
    //                               or if from == to (self-loops are rejected).
    void addRoad(int from, int to, double weight, bool bidirectional = true);

    bool hasNode(int id) const noexcept;
    std::size_t nodeCount() const noexcept { return nodes_.size(); }
    std::size_t roadCount() const noexcept { return roads_.size(); }

    // Both throw std::out_of_range for an unknown id.
    const Node& node(int id) const;
    const std::vector<Edge>& neighbors(int id) const;

    const std::vector<Node>& nodes() const noexcept { return nodes_; }
    const std::vector<Road>& roads() const noexcept { return roads_; }

    // Straight-line distance between the coordinates of two nodes.
    double euclideanDistance(int a, int b) const;

    // The A* heuristic is  h(n) = heuristicScale() * euclideanDistance(n, goal).
    //
    // Euclidean distance is only a valid ("admissible and consistent")
    // heuristic when no road is shorter than the straight line between its
    // endpoints. heuristicScale() is the smallest value of
    // (road weight / straight-line length), capped at 1.0, so the scaled
    // heuristic can never overestimate a real road cost - whatever weights
    // the graph uses. For the sample city every road is at least as long as
    // the straight line, so the scale is exactly 1.0.
    double heuristicScale() const noexcept { return heuristicScale_; }
    double heuristic(int from, int goal) const;

    // The predefined sample city (15 intersections, two of them cut off from
    // the rest so that "unreachable" can be demonstrated).
    static Graph createSampleCity();

private:
    std::vector<Node> nodes_;
    std::vector<std::vector<Edge>> adjacency_;
    std::vector<Road> roads_;
    double heuristicScale_ = 1.0;
};

}  // namespace nav
