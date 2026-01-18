#pragma once
#include "core.hpp"
#include <vector>
#include <memory>
#include <map>
#include <random>
#include <optional>

// Конфигурация генератора задач
struct GeneratorConfig {
    int width;
    int height;
    int min_shape_size;                    // Минимальный размер одной фигуры (в клетках)
    int max_shape_size;                    // Максимальный размер одной фигуры
    int min_bundle_area = 15;              // Мин. общая площадь набора фигур (бандла)
    int max_bundle_area = 25;              // Макс. общая площадь набора фигур
    GridType grid_type = GridType::SQUARE;
};

class PuzzleGenerator {
public:
    explicit PuzzleGenerator(const GeneratorConfig& cfg);

    Puzzle generate();

private:
    GeneratorConfig config;
    int piece_counter = 0; 
    std::mt19937 rng;


    // Методы создания разных типов сеток 
    std::shared_ptr<Grid> create_square_grid();
    std::shared_ptr<Grid> create_hex_grid();
    std::shared_ptr<Grid> create_triangle_grid();

    // Превращает набор клеток  в отдельный объект Figure
    std::shared_ptr<Figure> subset_to_figure(std::string name, const std::vector<int>& node_ids, std::shared_ptr<Grid> grid);

    // Внутренняя структура для промежуточного хранения фигур
    struct TempShape {
        std::shared_ptr<Figure> graph;
        std::vector<int> cells;
        int area;
    };
    
    //Выращивание одного региона (Random Walk / BFS), true - если вышло
    std::optional<std::vector<int>> grow_region(int start_node, int target_size, 
        std::shared_ptr<Grid> grid, 
        std::vector<char>& is_free);

    // Слияние мелких фигур: возвращает новый список фигур
    std::vector<TempShape> merge_small_shapes(const std::vector<TempShape>& shapes, std::shared_ptr<Grid> grid);

    std::vector<Bundle> create_bundles(std::vector<TempShape>& shapes, std::shared_ptr<Grid> grid);
};
