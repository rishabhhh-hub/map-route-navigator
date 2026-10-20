// Api.h - REST API layer.
//
// Two layers on purpose:
//   1. api::health / api::graphJson / api::route are plain functions that turn
//      a request into (HTTP status, JSON text). They know nothing about
//      sockets, so they can be unit-tested directly.
//   2. ApiServer wires those functions to real HTTP endpoints (cpp-httplib)
//      and serves the three frontend files.
#pragma once

#include <memory>
#include <string>

#include "Graph.h"

namespace nav {

namespace api {

inline constexpr const char* kApiVersion = "1.0.0";

struct Response {
    int status;        // HTTP status code
    std::string body;  // JSON text
};

// GET /api/health
Response health(const Graph& graph);

// GET /api/graph
Response graphJson(const Graph& graph);

// POST /api/route - `requestBody` is the raw request body (JSON text).
Response route(const Graph& graph, const std::string& requestBody);

}  // namespace api

class ApiServer {
public:
    // `frontendDir` is the folder that contains index.html, style.css and
    // script.js. Only those three files are ever served from it.
    ApiServer(Graph graph, std::string frontendDir);
    ~ApiServer();

    ApiServer(const ApiServer&) = delete;
    ApiServer& operator=(const ApiServer&) = delete;

    // Binds to host:port and serves requests until stop() is called.
    // Returns false (without blocking) if the port could not be bound.
    bool listen(const std::string& host, int port);
    void stop();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace nav
