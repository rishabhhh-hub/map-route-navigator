#include "Graph.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace nav {

int Graph::addNode(const std::string& name, double x, double y) {
    if (!std::isfinite(x) || !std::isfinite(y)) {
        throw std::invalid_argument("node coordinates must be finite numbers");
    }
    const int id = static_cast<int>(nodes_.size());
    nodes_.push_back(Node{id, name, x, y});
    adjacency_.emplace_back();
    return id;
}

void Graph::addRoad(int from, int to, double weight, bool bidirectional) {
    if (!hasNode(from) || !hasNode(to)) {
        throw std::out_of_range("addRoad: unknown node id");
    }
    if (from == to) {
        throw std::invalid_argument("addRoad: self-loops are not allowed");
    }
    if (!std::isfinite(weight) || weight < 0.0) {
        throw std::invalid_argument("addRoad: weight must be finite and non-negative");
    }

    adjacency_[static_cast<std::size_t>(from)].push_back(Edge{to, weight});
    if (bidirectional) {
        adjacency_[static_cast<std::size_t>(to)].push_back(Edge{from, weight});
    }
    roads_.push_back(Road{from, to, weight, bidirectional});

    // Keep the heuristic safe: never let it exceed (weight / straight line).
    const double straight = euclideanDistance(from, to);
    if (straight > 1e-12) {
        heuristicScale_ = std::min(heuristicScale_, weight / straight);
    }
}

bool Graph::hasNode(int id) const noexcept {
    return id >= 0 && static_cast<std::size_t>(id) < nodes_.size();
}

const Node& Graph::node(int id) const {
    if (!hasNode(id)) throw std::out_of_range("unknown node id");
    return nodes_[static_cast<std::size_t>(id)];
}

const std::vector<Edge>& Graph::neighbors(int id) const {
    if (!hasNode(id)) throw std::out_of_range("unknown node id");
    return adjacency_[static_cast<std::size_t>(id)];
}

double Graph::euclideanDistance(int a, int b) const {
    const Node& na = node(a);
    const Node& nb = node(b);
    return std::hypot(na.x - nb.x, na.y - nb.y);
}

double Graph::heuristic(int from, int goal) const {
    return heuristicScale_ * euclideanDistance(from, goal);
}

Graph Graph::createSampleCity() {
    Graph g;

    // Coordinates are in "map units"; the frontend scales them to fit the
    // screen. Node ids are assigned in this order (0..14).
    g.addNode("Central Station", 450, 280);  //  0
    g.addNode("North Gate", 430, 60);        //  1
    g.addNode("Market Square", 260, 170);    //  2
    g.addNode("Old Town", 90, 250);          //  3
    g.addNode("University", 640, 140);       //  4
    g.addNode("City Hospital", 810, 80);     //  5
    g.addNode("Riverside Park", 620, 330);   //  6
    g.addNode("Tech Park", 820, 290);        //  7
    g.addNode("Harbor", 760, 480);           //  8
    g.addNode("Stadium", 480, 490);          //  9
    g.addNode("West End", 110, 470);         // 10
    g.addNode("South Mall", 290, 400);       // 11
    g.addNode("Lakeview", 250, 40);          // 12
    g.addNode("Ferry Pier", 870, 380);       // 13  (not connected to the city)
    g.addNode("Island Depot", 880, 530);     // 14  (only reachable from Ferry Pier)

    // Road weights are road lengths in map units. Every road is at least as
    // long as the straight line between its two ends, which is what makes the
    // Euclidean heuristic safe for A*.
    struct RoadSpec { int from; int to; double weight; };
    static const RoadSpec roads[] = {
        {0, 1, 243},  {0, 2, 253},  {0, 6, 187},  {0, 9, 255},  {0, 11, 250},
        {0, 4, 307},  {1, 2, 223},  {1, 4, 270},  {1, 12, 209}, {2, 12, 157},
        {2, 3, 207},  {2, 11, 302}, {3, 10, 266}, {3, 11, 350}, {10, 11, 223},
        {11, 9, 253}, {4, 5, 199},  {4, 6, 220},  {5, 7, 263},  {6, 7, 225},
        {6, 8, 267},  {7, 8, 240},  {8, 9, 309},  {6, 9, 319},
        {13, 14, 166},  // the island pair: a separate component
    };
    for (const RoadSpec& r : roads) {
        g.addRoad(r.from, r.to, r.weight);  // undirected by default
    }
    return g;
}

}  // namespace nav
