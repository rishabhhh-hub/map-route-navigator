// main.cpp - entry point of the Map Route Navigator server.
//
//   map_route_navigator [--host 127.0.0.1] [--port 8080] [--frontend <dir>]

#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "Api.h"
#include "Graph.h"

namespace {

void printUsage() {
    std::cout << "Map Route Navigator - C++ pathfinding server\n\n"
                 "Usage: map_route_navigator [options]\n"
                 "  --host <address>   Address to bind (default 127.0.0.1, local machine only)\n"
                 "  --port <number>    TCP port (default 8080)\n"
                 "  --frontend <dir>   Folder with index.html, style.css, script.js\n"
                 "                     (default: ./frontend, ../frontend or ../../frontend)\n"
                 "  --help             Show this help\n";
}

bool fileExists(const std::string& path) {
    return std::ifstream(path).good();
}

// Finds the frontend folder when --frontend was not given, so the program
// works when started from the project root or from the build folder.
std::string findFrontendDir() {
    const std::vector<std::string> candidates = {"frontend", "../frontend", "../../frontend"};
    for (const std::string& dir : candidates) {
        if (fileExists(dir + "/index.html")) return dir;
    }
    return "frontend";  // the server reports a clear error if it is missing
}

}  // namespace

int main(int argc, char** argv) {
    std::string host = "127.0.0.1";
    int port = 8080;
    std::string frontendDir;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const bool hasValue = i + 1 < argc;
        if (arg == "--help" || arg == "-h") {
            printUsage();
            return 0;
        } else if (arg == "--host" && hasValue) {
            host = argv[++i];
        } else if (arg == "--port" && hasValue) {
            try {
                port = std::stoi(argv[++i]);
            } catch (const std::exception&) {
                port = -1;
            }
            if (port < 1 || port > 65535) {
                std::cerr << "Error: --port must be a number between 1 and 65535.\n";
                return 2;
            }
        } else if (arg == "--frontend" && hasValue) {
            frontendDir = argv[++i];
        } else {
            std::cerr << "Error: unknown or incomplete option '" << arg << "'.\n\n";
            printUsage();
            return 2;
        }
    }
    if (frontendDir.empty()) frontendDir = findFrontendDir();

    if (!fileExists(frontendDir + "/index.html")) {
        std::cerr << "Warning: " << frontendDir
                  << "/index.html not found. The API will work, but the web page will not load.\n"
                     "         Run the program from the project folder or use --frontend <dir>.\n";
    }

    nav::ApiServer server(nav::Graph::createSampleCity(), frontendDir);

    std::cout << "Map Route Navigator\n"
              << "  Frontend folder : " << frontendDir << "\n"
              << "  Listening on    : http://" << host << ":" << port << "\n"
              << "  Press Ctrl+C to stop.\n"
              << std::flush;

    if (!server.listen(host, port)) {
        std::cerr << "Error: could not bind to " << host << ":" << port
                  << ". Is another program already using that port? Try --port 8081.\n";
        return 1;
    }
    return 0;
}
