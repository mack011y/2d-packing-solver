#pragma once
#include "core.hpp"
#include <vector>
#include <memory>

// Типы доступных эвристик (стратегий размещения)
enum HeuristicType {
    MAX_CONTACT = 0,    // Максимизация числа соседей (сборка пазла)
    BOTTOM_LEFT = 1,    // Классический "Тетрис" (левее и ниже)
    MIN_HOLES = 2,      // Минимизация "дыр" (пустых соседей)
    WALL_HUGGING = 3,   // "Вдоль стен": приоритет краям поля
    HEURISTIC_COUNT = 4
};

// Класс для оценки качества размещения фигур
class Heuristics {
public:
    // Оценка "качества" позиции.
    // Возвращает числовой score. Чем больше, тем лучше позиция.
    // occupied_mask: Векторная маска всего поля (1 = занято, 0 = свободно). Размер = W*H.
    // footprint: список ID клеток, которые займет фигура.
    static float evaluate(
        HeuristicType type,
        const std::shared_ptr<Grid>& grid,
        const std::vector<char>& occupied_mask, // Быстрая проверка O(1)
        const std::vector<int>& footprint 
    );

    // Оптимизация поиска: возвращает список перспективных точек (якорей) для проверки.
    // occupied_mask: маска занятости поля.
    static std::vector<int> get_candidates(
        HeuristicType type,
        const std::shared_ptr<Grid>& grid,
        const std::vector<char>& occupied_mask
    );
};
