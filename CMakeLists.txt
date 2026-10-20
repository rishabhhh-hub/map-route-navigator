cmake_minimum_required(VERSION 3.16)
project(MapRouteNavigator VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optimised build unless the user asks for something else.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(Threads REQUIRED)

# ---------------------------------------------------------------------------
# Core library: graph, algorithms and API layer. Compiled once and shared by
# the server executable and the test executable.
# ---------------------------------------------------------------------------
add_library(navigator_core STATIC
  backend/Graph.cpp
  backend/Algorithms.cpp
  backend/Api.cpp
)
target_include_directories(navigator_core PUBLIC backend)
# Third-party headers (cpp-httplib, nlohmann/json) are vendored in third_party/.
# SYSTEM silences warnings that come from code we do not own.
target_include_directories(navigator_core SYSTEM PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/httplib
)
target_link_libraries(navigator_core PUBLIC Threads::Threads)

if(WIN32)
  # cpp-httplib needs Windows 7+ APIs and the Winsock library.
  target_compile_definitions(navigator_core PUBLIC _WIN32_WINNT=0x0A00)
  target_link_libraries(navigator_core PUBLIC ws2_32 wsock32)
endif()

if(MSVC)
  target_compile_options(navigator_core PRIVATE /W4)
else()
  target_compile_options(navigator_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------------------
# Server
# ---------------------------------------------------------------------------
add_executable(map_route_navigator backend/main.cpp)
target_link_libraries(map_route_navigator PRIVATE navigator_core)

# ---------------------------------------------------------------------------
# Tests (run with: ctest --test-dir build --output-on-failure)
# ---------------------------------------------------------------------------
enable_testing()
add_executable(algorithm_tests tests/algorithm_tests.cpp)
target_link_libraries(algorithm_tests PRIVATE navigator_core)
add_test(NAME algorithm_tests COMMAND algorithm_tests)
