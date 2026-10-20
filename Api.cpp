#include "Api.h"

#include <climits>
#include <cmath>
#include <exception>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "Algorithms.h"
#include "httplib.h"

namespace nav {

using json = nlohmann::json;

namespace {

// Serialise without ever throwing on odd bytes in a string.
std::string dumpJson(const json& j) {
    return j.dump(-1, ' ', false, json::error_handler_t::replace);
}

}  // namespace

namespace api {

namespace {

Response errorResponse(int status, const std::string& code,
                       const std::string& message) {
    return Response{status, dumpJson(json{{"error", code}, {"message", message}})};
}

// Reads body[field] as a node id. Returns an error Response if the field is
// missing, not an integer, or not a node of the graph; otherwise std::nullopt-
// style: `error` stays unset and `out` is filled.
bool readNodeId(const Graph& graph, const json& body, const char* field, int& out,
                Response& error) {
    const auto it = body.find(field);
    if (it == body.end()) {
        error = errorResponse(400, "missing_field",
                              std::string("Missing required field '") + field + "'.");
        return false;
    }
    const json& value = *it;

    bool ok = false;
    if (value.is_number_unsigned()) {
        ok = value.get<unsigned long long>() <= static_cast<unsigned long long>(INT_MAX);
        if (ok) out = static_cast<int>(value.get<unsigned long long>());
    } else if (value.is_number_integer()) {
        // Signed and negative (non-negative integers are "unsigned" above).
        ok = false;
    } else if (value.is_number_float() && std::isfinite(value.get<double>()) &&
               std::fabs(value.get<double>()) > static_cast<double>(INT_MAX)) {
        // An integer literal too large for 64 bits is parsed as a float.
        ok = false;
    } else {
        error = errorResponse(400, "invalid_field_type",
                              std::string("Field '") + field + "' must be an integer node id.");
        return false;
    }

    if (!ok || !graph.hasNode(out)) {
        error = errorResponse(
            400, "invalid_node",
            std::string("Field '") + field + "' is not a valid node id. Valid ids are 0 to " +
                std::to_string(graph.nodeCount() - 1) + ".");
        return false;
    }
    return true;
}

json routeResultToJson(const RouteResult& r) {
    json j;
    j["algorithm"] = r.algorithm;
    j["start"] = r.start;
    j["destination"] = r.destination;
    j["path"] = r.path;
    j["distance"] = r.reachable ? json(r.distance) : json(nullptr);
    j["exploredOrder"] = r.exploredOrder;
    j["nodesExplored"] = r.exploredOrder.size();
    j["executionTimeMs"] = r.executionTimeMs;
    j["reachable"] = r.reachable;
    return j;
}

}  // namespace

Response health(const Graph& graph) {
    return Response{200, dumpJson(json{{"status", "ok"},
                                       {"apiVersion", kApiVersion},
                                       {"nodes", graph.nodeCount()},
                                       {"roads", graph.roadCount()}})};
}

Response graphJson(const Graph& graph) {
    json nodes = json::array();
    for (const Node& n : graph.nodes()) {
        nodes.push_back({{"id", n.id}, {"name", n.name}, {"x", n.x}, {"y", n.y}});
    }
    json edges = json::array();
    for (const Road& r : graph.roads()) {
        edges.push_back({{"from", r.from},
                         {"to", r.to},
                         {"weight", r.weight},
                         {"bidirectional", r.bidirectional}});
    }
    return Response{200, dumpJson(json{{"nodes", nodes},
                                       {"edges", edges},
                                       {"heuristicScale", graph.heuristicScale()}})};
}

Response route(const Graph& graph, const std::string& requestBody) {
    // 1. Parse (without exceptions) and check the shape of the request.
    const json body = json::parse(requestBody, nullptr, /*allow_exceptions=*/false);
    if (body.is_discarded()) {
        return errorResponse(400, "malformed_json", "The request body is not valid JSON.");
    }
    if (!body.is_object()) {
        return errorResponse(400, "invalid_request", "The request body must be a JSON object.");
    }

    // 2. Validate the fields.
    Response error{0, ""};
    int start = -1;
    int destination = -1;
    if (!readNodeId(graph, body, "start", start, error)) return error;
    if (!readNodeId(graph, body, "destination", destination, error)) return error;

    const auto algIt = body.find("algorithm");
    if (algIt == body.end()) {
        return errorResponse(400, "missing_field", "Missing required field 'algorithm'.");
    }
    if (!algIt->is_string()) {
        return errorResponse(400, "invalid_field_type",
                             "Field 'algorithm' must be a string: \"dijkstra\" or \"astar\".");
    }
    const std::string algorithm = algIt->get<std::string>();

    // 3. Run the real C++ algorithm.
    RouteResult result;
    try {
        if (algorithm == "dijkstra") {
            result = runDijkstra(graph, start, destination);
        } else if (algorithm == "astar") {
            result = runAStar(graph, start, destination);
        } else {
            return errorResponse(400, "unsupported_algorithm",
                                 "Unsupported algorithm. Use \"dijkstra\" or \"astar\".");
        }
    } catch (const std::exception& e) {
        return errorResponse(500, "internal_error", e.what());
    }

    // 4. Build the response. An unreachable destination is reported as 404,
    //    but the body still carries the full result so a client can show it.
    json out = routeResultToJson(result);
    if (!result.reachable) {
        out["error"] = "no_route";
        out["message"] = "No route exists between node " + std::to_string(start) +
                         " and node " + std::to_string(destination) + ".";
        return Response{404, dumpJson(out)};
    }
    return Response{200, dumpJson(out)};
}

}  // namespace api

// ---------------------------------------------------------------------------
// HTTP server
// ---------------------------------------------------------------------------

struct ApiServer::Impl {
    Graph graph;
    std::string frontendDir;
    httplib::Server server;

    Impl(Graph g, std::string dir) : graph(std::move(g)), frontendDir(std::move(dir)) {}
};

namespace {

void sendJson(httplib::Response& res, const api::Response& r) {
    res.status = r.status;
    res.set_content(r.body, "application/json; charset=utf-8");
}

// Serves one of the fixed frontend files. `fileName` is always a string
// literal chosen below - no part of the request ever reaches the file path, so
// path traversal ("../") is impossible by construction.
void sendStaticFile(httplib::Response& res, const std::string& dir,
                    const char* fileName, const char* contentType) {
    std::ifstream in(dir + "/" + fileName, std::ios::binary);
    if (!in) {
        res.status = 500;
        res.set_content("Frontend file not found: " + std::string(fileName) +
                            ". Start the server from the project folder or pass --frontend <dir>.",
                        "text/plain; charset=utf-8");
        return;
    }
    const std::string content((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>());
    res.set_content(content, contentType);
}

}  // namespace

ApiServer::ApiServer(Graph graph, std::string frontendDir)
    : impl_(std::make_unique<Impl>(std::move(graph), std::move(frontendDir))) {
    Impl* d = impl_.get();
    httplib::Server& svr = d->server;

    // Requests are tiny JSON objects; refuse anything large.
    svr.set_payload_max_length(4096);
    svr.set_read_timeout(5, 0);
    svr.set_write_timeout(5, 0);

    // --- API ---------------------------------------------------------------
    svr.Get("/api/health", [d](const httplib::Request&, httplib::Response& res) {
        sendJson(res, api::health(d->graph));
    });
    svr.Get("/api/graph", [d](const httplib::Request&, httplib::Response& res) {
        sendJson(res, api::graphJson(d->graph));
    });
    svr.Post("/api/route", [d](const httplib::Request& req, httplib::Response& res) {
        sendJson(res, api::route(d->graph, req.body));
    });

    // --- Frontend (fixed whitelist of three files) ---------------------------
    const auto serveIndex = [d](const httplib::Request&, httplib::Response& res) {
        sendStaticFile(res, d->frontendDir, "index.html", "text/html; charset=utf-8");
    };
    svr.Get("/", serveIndex);
    svr.Get("/index.html", serveIndex);
    svr.Get("/style.css", [d](const httplib::Request&, httplib::Response& res) {
        sendStaticFile(res, d->frontendDir, "style.css", "text/css; charset=utf-8");
    });
    svr.Get("/script.js", [d](const httplib::Request&, httplib::Response& res) {
        sendStaticFile(res, d->frontendDir, "script.js", "application/javascript; charset=utf-8");
    });

    // --- Cross-cutting behaviour ---------------------------------------------
    // Security headers on every response.
    svr.set_post_routing_handler([](const httplib::Request&, httplib::Response& res) {
        res.set_header("X-Content-Type-Options", "nosniff");
        res.set_header("X-Frame-Options", "DENY");
        res.set_header("Referrer-Policy", "no-referrer");
        res.set_header("Cache-Control", "no-store");
        res.set_header("Content-Security-Policy",
                       "default-src 'self'; style-src 'self' 'unsafe-inline'; "
                       "img-src 'self' data:; frame-ancestors 'none'");
    });

    // Unknown URLs, wrong methods, oversized bodies: answer /api/* with JSON.
    svr.set_error_handler([](const httplib::Request& req, httplib::Response& res) {
        if (!res.body.empty()) return;  // a handler already wrote a message
        if (req.path.rfind("/api/", 0) == 0) {
            std::string message;
            if (res.status == 413) {
                message = "The request body is too large (limit: 4096 bytes).";
            } else if (res.status == 404) {
                message = "No such API endpoint, or the HTTP method is not supported for it.";
            } else {
                message = "The request could not be processed (HTTP status " +
                          std::to_string(res.status) + ").";
            }
            res.set_content(
                dumpJson(json{{"error", "http_error"}, {"message", message}}),
                "application/json; charset=utf-8");
        } else {
            res.set_content("Error " + std::to_string(res.status),
                            "text/plain; charset=utf-8");
        }
    });

    // Anything that slips through becomes a JSON 500 instead of a crash.
    svr.set_exception_handler(
        [](const httplib::Request&, httplib::Response& res, std::exception_ptr) {
            res.status = 500;
            res.set_content(
                dumpJson(json{{"error", "internal_error"},
                              {"message", "Unexpected server error."}}),
                "application/json; charset=utf-8");
        });
}

ApiServer::~ApiServer() = default;

bool ApiServer::listen(const std::string& host, int port) {
    if (!impl_->server.bind_to_port(host, port)) return false;
    impl_->server.listen_after_bind();
    return true;
}

void ApiServer::stop() { impl_->server.stop(); }

}  // namespace nav
