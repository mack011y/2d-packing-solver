#pragma once
#include <chrono>

// Простой таймер для замера производительности
class Timer {
private:
    std::chrono::high_resolution_clock::time_point start_time;

public:
    Timer() { start(); }

    void start() { 
        start_time = std::chrono::high_resolution_clock::now(); 
    }

    // Возвращает время в секундах, прошедшее с момента старта
    double get_elapsed_sec() const {
        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff = end_time - start_time;
        return diff.count();
    }
};
