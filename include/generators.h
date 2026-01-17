#pragma once
#include "core.hpp"
#include <vector>
#include <memory>
#include <map>

// Конфигурация генератора задач
struct GeneratorConfig {
    int width;                             // Ширина сетки
    int height;                            // Высота сетки
    int min_shape_size;                    // Минимальный размер одной фигуры (в клетках)
    int max_shape_size;                    // Максимальный размер одной фигуры
    int min_bundle_area = 15;              // Мин. общая площадь набора фигур (бандла)
    int max_bundle_area = 25;              // Макс. общая площадь набора фигур
    GridType grid_type = GridType::SQUARE; // Тип сетки
};

class PuzzleGenerator {
public:
    explicit PuzzleGenerator(const GeneratorConfig& cfg);

    // Основной метод генерации задачи
    // Создает новую сетку (out_grid) и возвращает:
    // 1. Список бандлов (фигур), которые нужно уложить.
    // 2. Карту правильного решения (ID клетки -> ID бандла), чтобы проверить, что решение существует.
    std::pair<std::vector<Bundle>, std::map<int, int>> generate(std::shared_ptr<Grid>& out_grid);

private:
    GeneratorConfig config;
    int piece_counter = 0; // Сквозной счетчик созданных фигур

    // Методы создания разных типов сеток (графов)
    std::shared_ptr<Grid> create_square_grid();
    std::shared_ptr<Grid> create_hex_grid();
    std::shared_ptr<Grid> create_triangle_grid();

    // Превращает набор клеток (подмножество сетки) в отдельный объект Figure
    std::shared_ptr<Figure> subset_to_figure(std::string name, const std::vector<int>& node_ids, std::shared_ptr<Grid> grid);
};
