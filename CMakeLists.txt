cmake_minimum_required(VERSION 3.10)
project(GraphTilingSolver)

set(CMAKE_CXX_STANDARD 17)

list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/sfml")
find_package(SFML 3 COMPONENTS Graphics Window System REQUIRED)

include_directories(include)

# Собираем исходники
file(GLOB SOURCES "src/*.cpp" "src/ui/*.cpp")

add_executable(solver_app ${SOURCES})

target_link_libraries(solver_app SFML::Graphics SFML::Window SFML::System)
