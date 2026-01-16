cmake_minimum_required(VERSION 3.14)
project(GraphTilingSolver)

set(CMAKE_CXX_STANDARD 17)

# SFML
list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/sfml")
find_package(SFML 3 COMPONENTS Graphics Window System REQUIRED)

# nlohmann/json
include(FetchContent)
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(json)

include_directories(include)

# Common sources
set(COMMON_SOURCES
    "src/generators.cpp"
    "src/solvers_common.cpp"
    "src/solvers_dlx.cpp"
    "src/solvers_grasp.cpp"
    "src/solvers_sa.cpp"
    "src/solvers_ga.cpp"
    "src/solvers_ga_perm.cpp"
    "src/heuristics.cpp"
)

# 1. Console Solver Tool
add_executable(solver_cli 
    src/main.cpp 
    ${COMMON_SOURCES}
)
target_link_libraries(solver_cli PRIVATE nlohmann_json::nlohmann_json)

# 2. Visualizer Tool
add_executable(solver_viewer 
    src/viewer_main.cpp 
    src/Viewer.cpp
    ${COMMON_SOURCES}
)
target_link_libraries(solver_viewer PRIVATE 
    nlohmann_json::nlohmann_json
    SFML::Graphics 
    SFML::Window 
    SFML::System
)

# 3. Debug Tool
add_executable(debug_tool 
    src/debug_main.cpp 
    ${COMMON_SOURCES}
)
target_link_libraries(debug_tool PRIVATE nlohmann_json::nlohmann_json)
