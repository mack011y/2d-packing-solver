#pragma once
#include "core.hpp"
#include <vector>
#include <set>
#include <memory>

// Типы доступных эвристик
enum HeuristicType {
    MAX_CONTACT = 0,    // Максимизация числа соседей
    BOTTOM_LEFT = 1,    // Классический "Тетрис" (левее и ниже)
    MIN_HOLES = 2,      // Минимизация "дыр" (пустых соседей)
    CENTER_GRAVITY = 3, // Тяготение к центру
    HEURISTIC_COUNT = 4
};

class Heuristics {
public:
    // Главная функция оценки. 
    // Возвращает "очки" за размещение фигуры в anchor_node с поворотом rotation.
    // Чем больше очков, тем лучше вариант.
    static float evaluate(
        HeuristicType type,
        const std::shared_ptr<Grid>& grid,
        const std::set<int>& occupied,
        const std::vector<int>& footprint // Список узлов, куда встанет фигура
    );

    // Вспомогательная функция для выбора кандидатов (оптимизация перебора).
    // Возвращает список узлов-якорей, которые имеет смысл проверять для данной стратегии.
    static std::vector<int> get_candidates(
        HeuristicType type,
        const std::shared_ptr<Grid>& grid,
        const std::set<int>& occupied
    );
};
