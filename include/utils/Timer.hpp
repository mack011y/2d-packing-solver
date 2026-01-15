#pragma once
#include <chrono>

class Timer {
    std::chrono::high_resolution_clock::time_point start_time;
public:
    void start() { start_time = std::chrono::high_resolution_clock::now(); }
    double get_elapsed_sec() {
        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff = end_time - start_time;
        return diff.count();
    }
};
