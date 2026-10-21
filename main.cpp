// main.cpp - the web server.
// It serves the web page and answers two questions from the browser:
//   GET  /api/graph  -> "what does the map look like?"
//   POST /api/route  -> "what is the shortest route?"  (runs Dijkstra or A*)

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "httplib.h"
#include "nlohmann/json.hpp"
#include "pathfinding.h"

using json = nlohmann::json;

// Read one of our three web page files. The file name is always chosen by
// us (never by the user), so nobody can ask the server for other files.
std::string readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    std::stringstream text;
    text << file.rdbuf();
    return text.str();
}

// Send an error message as JSON, for example {"error": "Unknown algorithm"}.
void sendError(httplib::Response& res, const std::string& message) {
    res.status = 400;
    res.set_content(json{{"error", message}}.dump(), "application/json");
}

int main() {
    Graph graph = makeSampleCity();

    // The web page files are in "frontend" (or "../frontend" if you run from build/).
    std::string dir = std::ifstream("frontend/index.html") ? "frontend" : "../frontend";

    httplib::Server server;

    // ----- the web page -----
    server.Get("/", [&](const httplib::Request&, httplib::Response& res) {
        res.set_content(readFile(dir + "/index.html"), "text/html");
    });
    server.Get("/style.css", [&](const httplib::Request&, httplib::Response& res) {
        res.set_content(readFile(dir + "/style.css"), "text/css");
    });
    server.Get("/script.js", [&](const httplib::Request&, httplib::Response& res) {
        res.set_content(readFile(dir + "/script.js"), "application/javascript");
    });

    // ----- GET /api/graph -----
    server.Get("/api/graph", [&](const httplib::Request&, httplib::Response& res) {
        json nodes = json::array();
        for (const Node& n : graph.nodes)
            nodes.push_back({{"id", n.id}, {"name", n.name}, {"x", n.x}, {"y", n.y}});

        json roads = json::array();
        for (const Road& r : graph.roads)
            roads.push_back({{"from", r.from}, {"to", r.to}, {"weight", r.weight}});

        res.set_content(json{{"nodes", nodes}, {"roads", roads}}.dump(), "application/json");
    });

    // ----- POST /api/route -----
    // Request:  {"start": 3, "destination": 5, "algorithm": "astar"}
    server.Post("/api/route", [&](const httplib::Request& req, httplib::Response& res) {
        // 1. Read the request and check it.
        json body = json::parse(req.body, nullptr, false);  // false = do not throw
        if (body.is_discarded() || !body.is_object())
            return sendError(res, "The request is not valid JSON.");

        if (!body.contains("start") || !body.contains("destination") || !body.contains("algorithm"))
            return sendError(res, "Send start, destination and algorithm.");

        if (!body["start"].is_number_integer() || !body["destination"].is_number_integer() ||
            !body["algorithm"].is_string())
            return sendError(res, "start and destination must be whole numbers, algorithm must be text.");

        int start = body["start"];
        int destination = body["destination"];
        std::string algorithm = body["algorithm"];

        if (!graph.validNode(start) || !graph.validNode(destination))
            return sendError(res, "That node number does not exist.");
        if (algorithm != "dijkstra" && algorithm != "astar")
            return sendError(res, "algorithm must be \"dijkstra\" or \"astar\".");

        // 2. Run the real algorithm.
        Result r = findRoute(graph, start, destination, algorithm == "astar");

        // 3. Send the answer back.
        json answer = {
            {"algorithm", algorithm},
            {"reachable", r.reachable},
            {"path", r.path},
            {"distance", r.distance},
            {"exploredOrder", r.explored},
            {"nodesExplored", r.explored.size()},
            {"executionTimeMs", r.timeMs},
        };
        res.set_content(answer.dump(), "application/json");
    });

    std::cout << "Open http://127.0.0.1:8080 in your browser (Ctrl+C to stop)\n";
    if (!server.listen("127.0.0.1", 8080)) {
        std::cerr << "Could not start. Is port 8080 already in use?\n";
        return 1;
    }
    return 0;
}
