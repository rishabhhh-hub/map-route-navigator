cmake_minimum_required(VERSION 3.16)
project(MapRouteNavigator CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

# The web server
add_executable(server backend/main.cpp)
target_include_directories(server PRIVATE backend third_party/httplib third_party)
target_link_libraries(server PRIVATE Threads::Threads)
if(WIN32)
  target_link_libraries(server PRIVATE ws2_32)
  target_compile_definitions(server PRIVATE _WIN32_WINNT=0x0A00)
endif()

# The tests
add_executable(tests tests/tests.cpp)
target_include_directories(tests PRIVATE backend)
