// algorithm_tests.cpp - self-contained tests (no test framework needed).
//
// Build and run through CMake:   ctest --test-dir build --output-on-failure
// or run the executable directly: ./build/algorithm_tests

#include <cmath>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "Algorithms.h"
#include "Api.h"
#include "Graph.h"

using nav::Graph;
using nav::RouteResult;
using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Minimal test harness
// ---------------------------------------------------------------------------
namespace {

int g_checks = 0;
int g_failures = 0;
std::string g_currentTest;

void reportFailure(const char* file, int line, const std::string& what) {
    ++g_failures;
    std::cerr << "  FAIL [" << g_currentTest << "] " << file << ":" << line << "  " << what << "\n";
}

#define CHECK(cond)                                              \
    do {                                                         \
        ++g_checks;                                              \
        if (!(cond)) reportFailure(__FILE__, __LINE__, #cond);   \
    } while (0)

#define CHECK_NEAR(a, b)                                                            \
    do {                                                                            \
        ++g_checks;                                                                 \
        const double va = (a), vb = (b);                                            \
        if (!(std::fabs(va - vb) < 1e-9))                                           \
            reportFailure(__FILE__, __LINE__,                                       \
                          std::string(#a " == " #b " (got ") + std::to_string(va) + \
                              " vs " + std::to_string(vb) + ")");                   \
    } while (0)

template <typename Ex>
bool throwsException(const std::function<void()>& fn) {
    try {
        fn();
    } catch (const Ex&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
using Path = std::vector<int>;

// Cost of a path, using the cheapest road between each consecutive pair.
// Returns -1 if some consecutive pair is not connected.
double pathCost(const Graph& g, const Path& path) {
    double total = 0.0;
    for (std::size_t i = 0; i + 1 < path.size(); ++i) {
        double best = std::numeric_limits<double>::infinity();
        for (const nav::Edge& e : g.neighbors(path[i])) {
            if (e.to == path[i + 1]) best = std::min(best, e.weight);
        }
        if (std::isinf(best)) return -1.0;
        total += best;
    }
    return total;
}

// Independent reference: Floyd-Warshall all-pairs shortest distances.
std::vector<std::vector<double>> floydWarshall(const Graph& g) {
    const std::size_t n = g.nodeCount();
    const double inf = std::numeric_limits<double>::infinity();
    std::vector<std::vector<double>> d(n, std::vector<double>(n, inf));
    for (std::size_t i = 0; i < n; ++i) {
        d[i][i] = 0.0;
        for (const nav::Edge& e : g.neighbors(static_cast<int>(i))) {
            d[i][static_cast<std::size_t>(e.to)] =
                std::min(d[i][static_cast<std::size_t>(e.to)], e.weight);
        }
    }
    for (std::size_t k = 0; k < n; ++k)
        for (std::size_t i = 0; i < n; ++i)
            for (std::size_t j = 0; j < n; ++j)
                if (d[i][k] + d[k][j] < d[i][j]) d[i][j] = d[i][k] + d[k][j];
    return d;
}

// A small graph with a known answer (road weights in brackets):
//
//   A --[4]-- B --[1]-- D
//   |         |         |
//  [1]       [2]       (C-D is [5])
//   |         |
//   C --------+
//
// Roads: A-B 4, A-C 1, C-B 2, B-D 1, C-D 5.
// Shortest A -> D is A-C-B-D with cost 1 + 2 + 1 = 4.
Graph makeKnownGraph() {
    Graph g;
    g.addNode("A", 0, 0);    // 0
    g.addNode("B", 2, 0);    // 1
    g.addNode("C", 0, 1);    // 2
    g.addNode("D", 3, 0);    // 3
    g.addRoad(0, 1, 4);      // A-B 4
    g.addRoad(0, 2, 1);      // A-C 1
    g.addRoad(2, 1, 2);      // C-B 2
    g.addRoad(1, 3, 1);      // B-D 1
    g.addRoad(2, 3, 5);      // C-D 5
    return g;
}

RouteResult run(const Graph& g, const std::string& alg, int s, int t) {
    return alg == "dijkstra" ? nav::runDijkstra(g, s, t) : nav::runAStar(g, s, t);
}

const char* const kAlgorithms[] = {"dijkstra", "astar"};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

void testKnownShortestPath() {
    const Graph g = makeKnownGraph();
    for (const char* alg : kAlgorithms) {
        const RouteResult r = run(g, alg, 0, 3);
        CHECK(r.reachable);
        CHECK_NEAR(r.distance, 4.0);
        CHECK((r.path == Path{0, 2, 1, 3}));
        CHECK(r.algorithm == alg);
        CHECK(r.executionTimeMs >= 0.0);
    }
}

void testDifferentWeightsChangeThePath() {
    // Triangle: S-T direct road vs. S-M-T detour.
    auto build = [](double direct) {
        Graph g;
        g.addNode("S", 0, 0);
        g.addNode("M", 5, 1);
        g.addNode("T", 10, 0);
        g.addRoad(0, 2, direct);  // direct
        g.addRoad(0, 1, 6);
        g.addRoad(1, 2, 6);       // detour total = 12
        return g;
    };
    for (const char* alg : kAlgorithms) {
        const RouteResult heavyDirect = run(build(20), alg, 0, 2);
        CHECK((heavyDirect.path == Path{0, 1, 2}));
        CHECK_NEAR(heavyDirect.distance, 12.0);

        const RouteResult lightDirect = run(build(11), alg, 0, 2);
        CHECK((lightDirect.path == Path{0, 2}));
        CHECK_NEAR(lightDirect.distance, 11.0);
    }
}

void testStartEqualsDestination() {
    const Graph g = Graph::createSampleCity();
    for (const char* alg : kAlgorithms) {
        const RouteResult r = run(g, alg, 4, 4);
        CHECK(r.reachable);
        CHECK_NEAR(r.distance, 0.0);
        CHECK((r.path == Path{4}));
        CHECK((r.exploredOrder == Path{4}));
    }
}

void testUnreachableDestination() {
    Graph g;
    g.addNode("A", 0, 0);
    g.addNode("B", 1, 0);
    g.addNode("Island", 50, 50);
    g.addRoad(0, 1, 1);
    for (const char* alg : kAlgorithms) {
        const RouteResult r = run(g, alg, 0, 2);
        CHECK(!r.reachable);
        CHECK(r.path.empty());
        CHECK(std::isinf(r.distance));
        CHECK(r.exploredOrder.size() == 2);  // explored everything it could reach
    }

    // Same thing on the shipped sample city (nodes 13 and 14 are an island).
    const Graph city = Graph::createSampleCity();
    for (const char* alg : kAlgorithms) {
        CHECK(!run(city, alg, 0, 13).reachable);
        CHECK(!run(city, alg, 14, 5).reachable);
        CHECK(run(city, alg, 13, 14).reachable);
    }
}

void testDirectedRoadsAreRespected() {
    Graph g;
    g.addNode("A", 0, 0);
    g.addNode("B", 1, 0);
    g.addRoad(0, 1, 1, /*bidirectional=*/false);  // one-way A -> B
    for (const char* alg : kAlgorithms) {
        CHECK(run(g, alg, 0, 1).reachable);
        CHECK(!run(g, alg, 1, 0).reachable);
    }
}

void testExploredOrderIsSane() {
    const Graph g = Graph::createSampleCity();
    for (const char* alg : kAlgorithms) {
        const RouteResult r = run(g, alg, 3, 5);
        CHECK(r.reachable);
        CHECK(!r.exploredOrder.empty());
        CHECK(r.exploredOrder.front() == 3);
        CHECK(r.exploredOrder.back() == 5);
        std::vector<int> seen(g.nodeCount(), 0);
        for (int id : r.exploredOrder) ++seen[static_cast<std::size_t>(id)];
        for (int count : seen) CHECK(count <= 1);  // no node settled twice
    }
}

void testAlgorithmsAgreeOnSampleCity() {
    const Graph g = Graph::createSampleCity();
    const auto reference = floydWarshall(g);
    const int n = static_cast<int>(g.nodeCount());
    for (int s = 0; s < n; ++s) {
        for (int t = 0; t < n; ++t) {
            const RouteResult d = nav::runDijkstra(g, s, t);
            const RouteResult a = nav::runAStar(g, s, t);
            const double ref = reference[static_cast<std::size_t>(s)][static_cast<std::size_t>(t)];
            CHECK(d.reachable == a.reachable);
            CHECK(d.reachable == !std::isinf(ref));
            if (d.reachable) {
                CHECK_NEAR(d.distance, a.distance);
                CHECK_NEAR(d.distance, ref);
                // The reported path really exists and really costs `distance`.
                CHECK(d.path.front() == s && d.path.back() == t);
                CHECK(a.path.front() == s && a.path.back() == t);
                CHECK_NEAR(pathCost(g, d.path), d.distance);
                CHECK_NEAR(pathCost(g, a.path), a.distance);
            }
        }
    }
}

// Random graphs, including weights SHORTER than the straight line (which
// forces the heuristic scale below 1) and some disconnected pieces.
void testAlgorithmsAgreeOnRandomGraphs() {
    std::mt19937 rng(12345);
    std::uniform_real_distribution<double> coord(0.0, 100.0);
    std::uniform_real_distribution<double> factor(0.3, 2.5);
    std::uniform_int_distribution<int> sizeDist(2, 25);

    for (int trial = 0; trial < 300; ++trial) {
        Graph g;
        const int n = sizeDist(rng);
        for (int i = 0; i < n; ++i) g.addNode("N" + std::to_string(i), coord(rng), coord(rng));
        std::uniform_int_distribution<int> pick(0, n - 1);
        const int roads = n + (trial % 7);  // sparse: some graphs stay disconnected
        for (int r = 0; r < roads; ++r) {
            const int a = pick(rng);
            const int b = pick(rng);
            if (a == b) continue;
            const bool twoWay = (rng() % 4) != 0;  // mostly undirected, some one-way
            g.addRoad(a, b, g.euclideanDistance(a, b) * factor(rng), twoWay);
        }

        const auto reference = floydWarshall(g);
        for (int s = 0; s < n; ++s) {
            for (int t = 0; t < n; ++t) {
                const RouteResult d = nav::runDijkstra(g, s, t);
                const RouteResult a = nav::runAStar(g, s, t);
                const double ref = reference[static_cast<std::size_t>(s)][static_cast<std::size_t>(t)];
                CHECK(d.reachable == a.reachable);
                CHECK(d.reachable == !std::isinf(ref));
                if (d.reachable) {
                    CHECK(std::fabs(d.distance - ref) < 1e-7);
                    CHECK(std::fabs(a.distance - ref) < 1e-7);
                    CHECK(std::fabs(pathCost(g, a.path) - a.distance) < 1e-7);
                }
            }
        }
    }
}

void testHeuristicIsAdmissibleAndConsistent() {
    // Sample city: scale must be exactly 1 (every road >= straight line).
    const Graph city = Graph::createSampleCity();
    CHECK_NEAR(city.heuristicScale(), 1.0);

    // A graph with a "too short" road: the scale must drop below 1.
    Graph g;
    g.addNode("A", 0, 0);
    g.addNode("B", 10, 0);
    g.addNode("C", 20, 0);
    g.addRoad(0, 1, 5);   // road is half the straight-line length
    g.addRoad(1, 2, 10);
    CHECK_NEAR(g.heuristicScale(), 0.5);

    const Graph* const graphs[] = {&city, &g};
    for (const Graph* graph : graphs) {
        const auto ref = floydWarshall(*graph);
        const int n = static_cast<int>(graph->nodeCount());
        for (int goal = 0; goal < n; ++goal) {
            for (int u = 0; u < n; ++u) {
                const double h = graph->heuristic(u, goal);
                const double truth = ref[static_cast<std::size_t>(u)][static_cast<std::size_t>(goal)];
                if (!std::isinf(truth)) CHECK(h <= truth + 1e-9);  // admissible
                for (const nav::Edge& e : graph->neighbors(u)) {
                    CHECK(h <= e.weight + graph->heuristic(e.to, goal) + 1e-9);  // consistent
                }
            }
        }
    }
}

void testInvalidNodeIdsInAlgorithms() {
    const Graph g = Graph::createSampleCity();
    const int n = static_cast<int>(g.nodeCount());
    for (const char* alg : kAlgorithms) {
        CHECK(throwsException<std::out_of_range>([&] { run(g, alg, -1, 0); }));
        CHECK(throwsException<std::out_of_range>([&] { run(g, alg, 0, n); }));
        CHECK(throwsException<std::out_of_range>([&] { run(g, alg, n + 100, 0); }));
    }
}

void testGraphRejectsBadInput() {
    Graph g;
    g.addNode("A", 0, 0);
    g.addNode("B", 1, 1);
    CHECK(throwsException<std::invalid_argument>([&] { g.addRoad(0, 1, -1.0); }));
    CHECK(throwsException<std::invalid_argument>([&] { g.addRoad(0, 1, std::nan("")); }));
    CHECK(throwsException<std::invalid_argument>(
        [&] { g.addRoad(0, 1, std::numeric_limits<double>::infinity()); }));
    CHECK(throwsException<std::invalid_argument>([&] { g.addRoad(0, 0, 1.0); }));
    CHECK(throwsException<std::out_of_range>([&] { g.addRoad(0, 5, 1.0); }));
    CHECK(throwsException<std::invalid_argument>([&] { g.addNode("X", std::nan(""), 0); }));
    CHECK(g.roadCount() == 0);  // nothing half-added
}

void testSampleCityShape() {
    const Graph g = Graph::createSampleCity();
    CHECK(g.nodeCount() >= 10);
    for (std::size_t i = 0; i < g.nodeCount(); ++i) {
        CHECK(g.node(static_cast<int>(i)).id == static_cast<int>(i));
        CHECK(!g.node(static_cast<int>(i)).name.empty());
    }
    for (const nav::Road& r : g.roads()) {
        CHECK(r.weight >= 0.0);
        CHECK(g.hasNode(r.from) && g.hasNode(r.to));
    }
}

// --- API layer (no sockets needed) -----------------------------------------

json parse(const nav::api::Response& r) { return json::parse(r.body); }

void testApiHealthAndGraph() {
    const Graph g = Graph::createSampleCity();
    const auto health = nav::api::health(g);
    CHECK(health.status == 200);
    CHECK(parse(health)["status"] == "ok");
    CHECK(parse(health)["apiVersion"] == nav::api::kApiVersion);

    const auto graph = nav::api::graphJson(g);
    CHECK(graph.status == 200);
    const json j = parse(graph);
    CHECK(j["nodes"].size() == g.nodeCount());
    CHECK(j["edges"].size() == g.roadCount());
    CHECK(j["nodes"][0].contains("name") && j["nodes"][0].contains("x") && j["nodes"][0].contains("y"));
    CHECK(j["edges"][0].contains("from") && j["edges"][0].contains("weight"));
}

void testApiRouteSuccess() {
    const Graph g = Graph::createSampleCity();
    for (const char* alg : kAlgorithms) {
        const json req = {{"start", 3}, {"destination", 5}, {"algorithm", alg}};
        const auto res = nav::api::route(g, req.dump());
        CHECK(res.status == 200);
        const json j = parse(res);
        CHECK(j["reachable"] == true);
        CHECK(j["algorithm"] == alg);
        CHECK(j["path"].front() == 3 && j["path"].back() == 5);
        CHECK(j["nodesExplored"] == j["exploredOrder"].size());
        CHECK(j["executionTimeMs"].is_number());
        const RouteResult direct = run(g, alg, 3, 5);
        CHECK_NEAR(j["distance"].get<double>(), direct.distance);  // API == algorithm
    }
}

void testApiRouteStartEqualsDestination() {
    const Graph g = Graph::createSampleCity();
    const auto res = nav::api::route(g, R"({"start":2,"destination":2,"algorithm":"astar"})");
    CHECK(res.status == 200);
    const json j = parse(res);
    CHECK(j["distance"] == 0.0);
    CHECK((j["path"] == json::array({2})));
}

void testApiRouteUnreachable() {
    const Graph g = Graph::createSampleCity();
    const auto res = nav::api::route(g, R"({"start":0,"destination":14,"algorithm":"dijkstra"})");
    CHECK(res.status == 404);
    const json j = parse(res);
    CHECK(j["error"] == "no_route");
    CHECK(j["reachable"] == false);
    CHECK(j["distance"].is_null());
    CHECK(j["path"].empty());
    CHECK(j["message"].is_string());
}

void expectError(const Graph& g, const std::string& body, int status, const std::string& code) {
    const auto res = nav::api::route(g, body);
    CHECK(res.status == status);
    const json j = json::parse(res.body, nullptr, false);
    CHECK(!j.is_discarded());
    if (!j.is_discarded()) {
        CHECK(j.value("error", "") == code);
        CHECK(j.contains("message"));
    }
}

void testApiRouteValidation() {
    const Graph g = Graph::createSampleCity();
    const int n = static_cast<int>(g.nodeCount());

    // Malformed JSON / wrong shape
    expectError(g, "", 400, "malformed_json");
    expectError(g, "{not json", 400, "malformed_json");
    expectError(g, "[1,2,3]", 400, "invalid_request");
    expectError(g, "42", 400, "invalid_request");

    // Missing fields
    expectError(g, R"({"destination":1,"algorithm":"astar"})", 400, "missing_field");
    expectError(g, R"({"start":1,"algorithm":"astar"})", 400, "missing_field");
    expectError(g, R"({"start":1,"destination":2})", 400, "missing_field");

    // Wrong types
    expectError(g, R"({"start":"1","destination":2,"algorithm":"astar"})", 400, "invalid_field_type");
    expectError(g, R"({"start":1.5,"destination":2,"algorithm":"astar"})", 400, "invalid_field_type");
    expectError(g, R"({"start":true,"destination":2,"algorithm":"astar"})", 400, "invalid_field_type");
    expectError(g, R"({"start":null,"destination":2,"algorithm":"astar"})", 400, "invalid_field_type");
    expectError(g, R"({"start":1,"destination":2,"algorithm":7})", 400, "invalid_field_type");

    // Invalid node ids
    expectError(g, R"({"start":-1,"destination":2,"algorithm":"astar"})", 400, "invalid_node");
    expectError(g, R"({"start":1,"destination":)" + std::to_string(n) + R"(,"algorithm":"astar"})",
                400, "invalid_node");
    expectError(g, R"({"start":99999999999999999999,"destination":2,"algorithm":"astar"})", 400,
                "invalid_node");

    // Unsupported algorithm (names are exact and lower-case)
    expectError(g, R"({"start":1,"destination":2,"algorithm":"bfs"})", 400, "unsupported_algorithm");
    expectError(g, R"({"start":1,"destination":2,"algorithm":"A*"})", 400, "unsupported_algorithm");
    expectError(g, R"({"start":1,"destination":2,"algorithm":""})", 400, "unsupported_algorithm");
}

struct TestCase {
    const char* name;
    void (*fn)();
};

}  // namespace

int main() {
    const TestCase tests[] = {
        {"known shortest path", testKnownShortestPath},
        {"different weights change the path", testDifferentWeightsChangeThePath},
        {"start equals destination", testStartEqualsDestination},
        {"unreachable destination", testUnreachableDestination},
        {"directed roads are respected", testDirectedRoadsAreRespected},
        {"explored order is sane", testExploredOrderIsSane},
        {"Dijkstra and A* agree (sample city, all pairs)", testAlgorithmsAgreeOnSampleCity},
        {"Dijkstra and A* agree (300 random graphs vs Floyd-Warshall)", testAlgorithmsAgreeOnRandomGraphs},
        {"heuristic is admissible and consistent", testHeuristicIsAdmissibleAndConsistent},
        {"invalid node ids throw", testInvalidNodeIdsInAlgorithms},
        {"graph rejects bad input", testGraphRejectsBadInput},
        {"sample city shape", testSampleCityShape},
        {"API health and graph", testApiHealthAndGraph},
        {"API route success", testApiRouteSuccess},
        {"API route start == destination", testApiRouteStartEqualsDestination},
        {"API route unreachable", testApiRouteUnreachable},
        {"API route validation errors", testApiRouteValidation},
    };

    int failedTests = 0;
    for (const TestCase& t : tests) {
        g_currentTest = t.name;
        const int before = g_failures;
        try {
            t.fn();
        } catch (const std::exception& e) {
            ++g_failures;
            std::cerr << "  FAIL [" << t.name << "] unexpected exception: " << e.what() << "\n";
        }
        const bool ok = g_failures == before;
        if (!ok) ++failedTests;
        std::cout << (ok ? "[ PASS ] " : "[ FAIL ] ") << t.name << "\n";
    }

    std::cout << "\n" << g_checks << " checks, " << g_failures << " failed, "
              << (sizeof(tests) / sizeof(tests[0]) - static_cast<std::size_t>(failedTests)) << "/"
              << (sizeof(tests) / sizeof(tests[0])) << " tests passed\n";
    return g_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
