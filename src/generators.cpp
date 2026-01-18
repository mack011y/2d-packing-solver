#include "generators.h"
#include "utils/ColorUtils.hpp"
#include <random>
#include <algorithm>
#include <set>
#include <iostream>
#include <queue>
#include <cmath>
#include <unordered_map>
#include <optional>

PuzzleGenerator::PuzzleGenerator(const GeneratorConfig& cfg) : config(cfg) {
    std::random_device rd;
    rng.seed(rd());
}

std::shared_ptr<Grid> PuzzleGenerator::create_square_grid() {
    auto g = std::make_shared<Grid>(config.width, config.height, GridType::SQUARE);
    
    for(int y=0; y<config.height; ++y) {
        for(int x=0; x<config.width; ++x) {
            g->add_node(GridCellData(x, y));
        }
    }
    
    for(int y=0; y<config.height; ++y) {
        for(int x=0; x<config.width; ++x) {
            int id = y*config.width + x;
            // Связь вправо (порт 1 у текущего, порт 3 у соседа)
            if(x < config.width - 1) g->add_edge(id, y*config.width+(x+1), 1, 3); 
            // Связь вниз (порт 2 у текущего, порт 0 у соседа)
            if(y < config.height - 1) g->add_edge(id, (y+1)*config.width+x, 2, 0); 
        }
    }
    return g;
}

std::shared_ptr<Grid> PuzzleGenerator::create_hex_grid() {
    auto g = std::make_shared<Grid>(config.width, config.height, GridType::HEXAGON);

    for(int y=0; y<config.height; ++y) {
        for(int x=0; x<config.width; ++x) {
            g->add_node(GridCellData(x, y));
        }
    }

    
    // Как уже обсуждалось ранее, соседи у шестиугольника определяются так же как и у квадрата, только со смешением, существует биекция
    int er_dx[] = {0, 1, 0, -1, -1, -1};
    int er_dy[] = {-1, 0, 1, 1, 0, -1};
    int or_dx[] = {1, 1, 1, 0, -1, 0};
    int or_dy[] = {-1, 0, 1, 1, 0, -1};

    for(int y=0; y<config.height; ++y) {
        for(int x=0; x<config.width; ++x) {
            int id = y*config.width + x;
            int* dx = (y % 2 == 0) ? er_dx : or_dx;
            int* dy = (y % 2 == 0) ? er_dy : or_dy;

            for(int p=0; p<6; ++p) {
                int nx = x + dx[p];
                int ny = y + dy[p];
                
                if (nx >= 0 && nx < config.width && ny >= 0 && ny < config.height) {
                    int nid = ny * config.width + nx;
                    // p - выходной порт, (p+3)%6 - входной (противоположный)
                    g->add_edge(id, nid, p, (p+3)%6);
                }
            }
        }
    }
    return g;
}

std::shared_ptr<Grid> PuzzleGenerator::create_triangle_grid() {
    auto g = std::make_shared<Grid>(config.width, config.height, GridType::TRIANGLE);
    
    for(int y=0; y<config.height; ++y) {
        for(int x=0; x<config.width; ++x) {
            g->add_node(GridCellData(x, y));
        }
    }

    for(int y=0; y<config.height; ++y) {
        for(int x=0; x<config.width; ++x) {
            int id = y*config.width + x;
            bool is_up = ((x + y) % 2 == 0); // Треугольники чередуются (острием вверх/вниз)
            
            // Горизонтальная связь
            if (x < config.width - 1) g->add_edge(id, y*config.width + (x+1), 0, 1); 
            
            // Вертикальная связь зависит от ориентации
            if (is_up) {
                if (y < config.height - 1) g->add_edge(id, (y+1)*config.width + x, 2, 2); 
            } else {
                if (y > 0) g->add_edge(id, (y-1)*config.width + x, 2, 2); 
            }
        }
    }
    return g;
}

// Преобразование набора клеток по коордам сетки в объект Figure сохраняя топологию
std::shared_ptr<Figure> PuzzleGenerator::subset_to_figure(std::string name, const std::vector<int>& node_ids, std::shared_ptr<Grid> grid) {
    // 1. Создаем пустую фигуру
    auto fig = std::make_shared<Figure>(name, grid->get_max_ports());
    
    // отображение для локального удобства (Сетка -> Фигура) понижение номеров/сжатие координат
    std::unordered_map<int, int> grid_to_fig;
    
    for (int gid : node_ids) {
        int fid = fig->add_node(); 
        grid_to_fig[gid] = fid;
    }
    
    for (int gid : node_ids) {
        const auto& g_node = grid->get_node(gid);
        int fid = grid_to_fig[gid];
        
        for (int p=0; p < grid->get_max_ports(); ++p) {
            int neighbor_gid = g_node.get_neighbor(p);
            
            if (neighbor_gid != -1 && grid_to_fig.count(neighbor_gid)) {
                int neighbor_fid = grid_to_fig[neighbor_gid];
                fig->add_directed_edge(fid, neighbor_fid, p); // мы перебираем ВСЕ ноды, поэтому можем не искать обратный порт
            }
        }
    }
    
    return fig;
}

// Выращивание одной фигуры 
std::optional<std::vector<int>> PuzzleGenerator::grow_region(int start_node, int target_size, std::shared_ptr<Grid> grid, std::vector<char>& is_free) {
    if (!is_free[start_node]) return std::nullopt;

    // Используем set для уникальности и vector для рандомного выбора
    // Но так как target_size маленький (обычно < 10), можно оптимизировать
    std::vector<int> current_shape = {start_node};
    std::vector<int> growth = {start_node}; 
    is_free[start_node] = 0; 
    
    // Вспомогательный set для быстрой проверки пренадлежности тикущщей фигуре
    std::unordered_map<int, bool> in_shape; 
    in_shape[start_node] = true;

    while((int)current_shape.size() < target_size && !growth.empty()) {
        int grow_from;
        if (std::uniform_real_distribution<>(0, 1)(rng) < 0.6) { // в приоритете сложность, а значит делаем змею, вероятнее идем из самой свежей вершины
            grow_from = growth.back();
        } else {
            std::uniform_int_distribution<> g_dist(0, growth.size()-1);
            grow_from = growth[g_dist(rng)];
        }

        // Ищем свободных соседей
        std::vector<int> valid_neighbors;
        for(int p=0; p < grid->get_max_ports(); ++p) {
            int n = grid->get_node(grow_from).get_neighbor(p);

            if (n != -1 && is_free[n] && !in_shape.count(n)) {
                valid_neighbors.push_back(n);
            }
        }

        if(valid_neighbors.empty()) {
            // ТПоскольку мы не нашли соседей, значит из этой вершины перейти в следующую уже нельзя, можно удалить
            if (std::find(growth.begin(), growth.end(), grow_from) != growth.end()) {
                    auto it = std::find(growth.begin(), growth.end(), grow_from);
                    if (it != growth.end()) growth.erase(it);
            }
            continue;
        }

        // Выбираем случайного соседа и добавляем в фигуру
        std::uniform_int_distribution<> n_dist(0, valid_neighbors.size()-1);
        int next = valid_neighbors[n_dist(rng)];
        
        current_shape.push_back(next);
        in_shape[next] = true;
        growth.push_back(next);
        is_free[next] = 0; 
    }
    
    return current_shape;
}

// 2. Слияние мелких остатков с соседями (Упрощенная версия: Поглощение)
std::vector<PuzzleGenerator::TempShape> PuzzleGenerator::merge_small_shapes(const std::vector<TempShape>& input_shapes, std::shared_ptr<Grid> grid) {
    std::vector<TempShape> shapes = input_shapes;
    
    // Карта: ID клетки -> Индекс фигуры в векторе shapes
    std::vector<int> cell_to_shape_idx(grid->size(), -1);
    
    // Инициализация карты
    for(size_t i=0; i<shapes.size(); ++i) {
        for(int cid : shapes[i].cells) {
            cell_to_shape_idx[cid] = (int)i;
        }
    }
    
    // Проходим по всем фигурам
    for(size_t i=0; i<shapes.size(); ++i) {
        // Если фигура уже удалена (пустая) или достаточно большая - пропускаем
        if (shapes[i].cells.empty() || shapes[i].area >= config.min_shape_size) {
            continue;
        }
        
        // Фигура маленькая - нужно слить с соседом
        // Ищем соседей
        std::vector<int> neighbor_indices;
        
        for(int cid : shapes[i].cells) {
            const auto& node = grid->get_node(cid);
            for(int n_cid : node.get_all_neighbors()) {
                if (n_cid != -1) {
                    int n_idx = cell_to_shape_idx[n_cid];
                    // Если сосед существует, не является нами самими и не удален
                    if (n_idx != -1 && n_idx != (int)i && !shapes[n_idx].cells.empty()) {
                        neighbor_indices.push_back(n_idx);
                    }
                }
            }
        }
        
        // Удаляем дубликаты соседей
        std::sort(neighbor_indices.begin(), neighbor_indices.end());
        neighbor_indices.erase(std::unique(neighbor_indices.begin(), neighbor_indices.end()), neighbor_indices.end());
        
        if (!neighbor_indices.empty()) {
            // Выбираем случайного соседа для слияния
            std::uniform_int_distribution<> dist(0, neighbor_indices.size() - 1);
            int target_idx = neighbor_indices[dist(rng)];
            
            // Сливаем i -> target_idx
            auto& src = shapes[i];
            auto& dst = shapes[target_idx];
            
            // Переносим клетки
            for(int cid : src.cells) {
                dst.cells.push_back(cid);
                cell_to_shape_idx[cid] = target_idx; // Обновляем карту
            }
            dst.area += src.area;
            
            // Очищаем источник
            src.cells.clear();
            src.area = 0;
        }
    }
    
    // Собираем только выжившие фигуры
    std::vector<TempShape> final_shapes;
    for(const auto& s : shapes) {
        if (!s.cells.empty()) {
            final_shapes.push_back(s);
        }
    }
    
    return final_shapes;
}

// 3. Формирование бандлов и раскраска
std::vector<Bundle> PuzzleGenerator::create_bundles(std::vector<TempShape>& shapes, std::shared_ptr<Grid> grid) {
    std::shuffle(shapes.begin(), shapes.end(), rng);
    
    std::vector<Bundle> bundles;
    size_t idx = 0;
    int bundle_counter = 0;
    
    std::uniform_int_distribution<> area_dist(
        config.min_bundle_area, 
        config.max_bundle_area
    );

    while(idx < shapes.size()) {
        int target_area = area_dist(rng);
        int current_bundle_area = 0;
        std::vector<std::shared_ptr<Figure>> group_shapes;
        
        while(idx < shapes.size()) {
             auto& item = shapes[idx];
             
             if (current_bundle_area > 0 && current_bundle_area >= target_area) {
                 break; 
             }
             
             group_shapes.push_back(item.graph);
             current_bundle_area += item.area;
             
             // Заполняем ID бандла в сетке
             for(int nid : item.cells) {
                 grid->get_node(nid).get_data().bundle_id = bundle_counter;
             }
             
             idx++;
        }

        if (group_shapes.empty()) break; 

        bundles.emplace_back(bundle_counter, group_shapes, Color{255, 255, 255});
        bundle_counter++;
    }
    
    // Раскраска (Heatmap based on area)
    size_t min_area = 1e9, max_area = 0;
    for(const auto& b : bundles) {
        size_t area = b.get_total_area();
        if(area < min_area) min_area = area;
        if(area > max_area) max_area = area;
    }
    
    for(auto& bundle : bundles) {
        float t = 0.0f;
        if (max_area > min_area) {
            t = (float)(bundle.get_total_area() - min_area) / (float)(max_area - min_area);
        }
        bundle.set_color(ColorUtils::get_heatmap_color(t));
    }
    
    return bundles;
}

// Основной метод генерации задачи
Puzzle PuzzleGenerator::generate() {
    piece_counter = 0;
    std::shared_ptr<Grid> out_grid;

    // 1. Создаем пустую сетку нужного типа
    if (config.grid_type == GridType::HEXAGON) {
        out_grid = create_hex_grid();
    } else if (config.grid_type == GridType::TRIANGLE) {
        out_grid = create_triangle_grid();
    } else {
        out_grid = create_square_grid();
    }

    // Оптимизация: Используем вектор для пула свободных узлов (для рандома)
    std::vector<int> available_nodes_pool;
    available_nodes_pool.reserve(out_grid->size());
    std::vector<char> node_is_free(out_grid->size(), 1); // 1 = свободно, 0 = занято

    for(size_t i=0; i<out_grid->size(); ++i) {
        available_nodes_pool.push_back(i);
    }

    std::vector<TempShape> shapes_data;
    
    // 2. Выращивание фигур (Partitioning)
    std::uniform_int_distribution<> size_dist(config.min_shape_size, config.max_shape_size);

    while(!available_nodes_pool.empty()) {
        // Быстрый выбор случайного:
        std::uniform_int_distribution<> dis(0, available_nodes_pool.size()-1);
        int rand_idx = dis(rng);
        int start = available_nodes_pool[rand_idx];
        
        // Swap-and-pop
        available_nodes_pool[rand_idx] = available_nodes_pool.back();
        available_nodes_pool.pop_back();

        // Попытка вырастить фигуру
        int target_size = size_dist(rng);
        
        if (auto new_cells = grow_region(start, target_size, out_grid, node_is_free)) {
            shapes_data.push_back({nullptr, *new_cells, (int)new_cells->size()});
        }
    }

    // 2.5 Слияние мелких остатков (Merge)
    // ТЕПЕРЬ ФУНКЦИЯ ВОЗВРАЩАЕТ НОВЫЙ СПИСОК ФИГУР
    shapes_data = merge_small_shapes(shapes_data, out_grid);
    
    // Создаем финальные объекты Figure (Graph objects)
    for(auto& shape : shapes_data) {
        auto fig = subset_to_figure("S_" + std::to_string(piece_counter), shape.cells, out_grid);
        shape.graph = fig;
        
        // Записываем ID фигуры в сетку (для валидации решения)
        for(int cid : shape.cells) {
            out_grid->get_node(cid).get_data().figure_id = piece_counter;
        }
        piece_counter++;
    }

    // 3. Группировка фигур в Бандлы (Bundles)
    std::vector<Bundle> bundles = create_bundles(shapes_data, out_grid);
    
    return Puzzle(out_grid, bundles, "Generated");
}
