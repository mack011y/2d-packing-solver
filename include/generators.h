#pragma once
#include "core.hpp"
#include <vector>
#include <memory>
#include <map>

struct GeneratorConfig {
    int width;
    int height;
    int min_shape_size;
    int max_shape_size;
    int min_bundle_area = 15;
    int max_bundle_area = 25;
    GridType grid_type = GridType::SQUARE;
};

class PuzzleGenerator {
public:
    explicit PuzzleGenerator(const GeneratorConfig& cfg);

    // Основной метод генерации
    // Создает сетку (out_grid) и возвращает Бандлы и Карту решения
    std::pair<std::vector<Bundle>, std::map<int, int>> generate(std::shared_ptr<Grid>& out_grid);

private:
    GeneratorConfig config;
    int piece_counter = 0;

    // Внутренние методы создания сеток
    std::shared_ptr<Grid> create_square_grid();
    std::shared_ptr<Grid> create_hex_grid();
    std::shared_ptr<Grid> create_triangle_grid();

    // Вспомогательный метод конвертации
    std::shared_ptr<Figure> subset_to_figure(std::string name, const std::vector<int>& node_ids, std::shared_ptr<Grid> grid);
};
