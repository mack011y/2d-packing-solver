#include "solvers.h"
#include <iostream>
#include <algorithm>
#include <vector>
#include <map>
#include <set>
#include <random>
#include <chrono>


// Функция оценки качества размещения, чем больше соседей тем лучш
int GRASPSolver::calculate_placement_score(const std::vector<int>& footprint, const std::vector<char>& occupied_mask) {
    int neighbors = 0;
    // Проходим по всем клеткам, которые займет фигура
    for (int nid : footprint) {
        const Node<GridCellData>& node = graph->get_node(nid);
        // Смотрим соседей этой клетки
        for(int neighbor_id : node.get_all_neighbors()) {
            if (neighbor_id != -1) {
                // Если сосед занят - это хорошо (фигуры прилипают друг к другу)
                if (occupied_mask[neighbor_id]) {
                    neighbors += 10;
                }
            }
        }
    }
    return neighbors;
}

// Рекурсивная функция размещения фигур (Backtracking with RCL)
// Реализует поиск в глубину с возвратом.
// Аргументы передаются по ссылке для избежания копирования тяжелых структур.
bool GRASPSolver::place_shapes_recursive(
    int shape_idx, 
    const std::vector<std::shared_ptr<Figure>>& shapes, 
    std::vector<char>& current_occupied_mask, 
    std::vector<SinglePlacement>& out_placements,
    std::mt19937& rng
) {
    // если все разместили
    if (shape_idx >= (int)shapes.size()) {
        return true; 
    }

    const std::shared_ptr<Figure>& shape = shapes[shape_idx];
    std::vector<SinglePlacement> candidates;
    
    // 1. Поиск всех возможных мест для текущей фигуры
    for(const auto& node : graph->get_nodes()) {
        int nid = node.get_id();
        // Если клетка уже занята, пропускаем (O(1) проверка)
        if (current_occupied_mask[nid]) {
            continue;
        }

        // Перебираем все возможные повороты фигуры
        for(int rot = 0; rot < graph->get_max_ports(); ++rot) {
            // Получаем "след" фигуры (список занимаемых клеток)
            // get_embedding возвращает пустой вектор, если фигура выходит за границы поля
            std::vector<int> fp = graph->get_embedding(shape, nid, rot);
            
            if (fp.empty()) {
                continue;
            }
            
            // Проверка на коллизии с уже установленными фигурами
            bool clash = false;
            for(int f_id : fp) {
                if (current_occupied_mask[f_id]) { 
                    clash = true; 
                    break; 
                }
            }
            
            if (!clash) {
                // Ход валиден. Вычисляем его эвристическую ценность.
                int score = calculate_placement_score(fp, current_occupied_mask);
                SinglePlacement placement;
                placement.figure = shape;
                placement.anchor = nid;
                placement.rotation = rot;
                placement.footprint = fp;
                placement.score = score;
                candidates.push_back(placement);
            }
        }
    }
    
    // Если кандидатов нет - тупик, возвращаемся назад
    if (candidates.empty()) {
        return false;
    }

    // 2. Построение RCL (Restricted Candidate List)
    // Это ключевой момент GRASP: мы берем не просто лучший вариант (Greedy),
    // а список "достаточно хороших" вариантов, чтобы добавить вариативность.
    int max_score = -9999;
    for(const auto& c : candidates) {
        if(c.score > max_score) {
            max_score = c.score;
        }
    }
    
    std::vector<SinglePlacement> rcl;
    float alpha = config.alpha; // Коэффициент жадности (обычно 0.8 - 0.9)
    for(const auto& c : candidates) {
        // Берем кандидатов, которые не хуже alpha * max_score
        if (c.score >= max_score * alpha || (max_score <= 0)) {
            rcl.push_back(c);
        }
    }
    
    // Случайный выбор из лучших кандидатов вносит стохастику, 
    // позволяя алгоритму выходить из локальных оптимумов.
    std::shuffle(rcl.begin(), rcl.end(), rng);
    
    // Ограничиваем ветвление: проверяем не более 5 лучших вариантов.
    // Это предотвращает комбинаторный взрыв при глубокой рекурсии.
    int max_tries = 5;
    if (rcl.size() < 5) {
        max_tries = (int)rcl.size();
    }
    
    for(int i = 0; i < max_tries; ++i) {
        const SinglePlacement& choice = rcl[i];
        
        // "Делаем ход": Создаем новую маску занятости для следующего шага рекурсии
        // Копирование вектора char (битовой маски) дешево по памяти.
        std::vector<char> new_occupied_mask = current_occupied_mask;
        for(int fid : choice.footprint) {
            new_occupied_mask[fid] = 1;
        }
        
        out_placements.push_back(choice);
        
        // Рекурсивный спуск (Depth-First Search)
        if (place_shapes_recursive(shape_idx + 1, shapes, new_occupied_mask, out_placements, rng)) {
            current_occupied_mask = new_occupied_mask; // Успех! Фиксируем изменения
            return true;
        }
        
        // Откат (Backtracking): Если ветка оказалась тупиковой, 
        // убираем фигуру из решения и пробуем следующего кандидата из RCL.
        out_placements.pop_back();
    }
    
    return false; // Ни один из вариантов не подошел
}

// Фаза построения решения (Construction Phase)
GRASPSolver::SolutionState GRASPSolver::run_construction_phase() {
    SolutionState state;
    
    // Сортировка наборов фигур (bundles): сначала пробуем разместить большие и сложные
    std::vector<int> bundle_indices(bundles.size());
    for(size_t i = 0; i < bundles.size(); ++i) {
        bundle_indices[i] = i;
    }
    
    // Лямбда-функция для сортировки
    std::sort(bundle_indices.begin(), bundle_indices.end(), [&](int a, int b) {
        if (bundles[a].get_total_area() != bundles[b].get_total_area()) {
            return bundles[a].get_total_area() > bundles[b].get_total_area();
        }
        return bundles[a].get_shapes().size() > bundles[b].get_shapes().size();
    });

    float current_score = 0.0f;
    state.placed_bundle_ids.clear();
    // Инициализация векторов (-1 означает свободно/не занято)
    state.node_allocations.assign(graph->size(), -1);
    state.node_figure_ids.assign(graph->size(), -1);
    
    // Векторная маска вместо сета
    std::vector<char> occupied_mask(graph->size(), 0);
    static std::random_device rd;
    static std::mt19937 g(rd());

    int fig_uid_counter = 0;

    // Проходим по всем наборам фигур
    for(int b_idx : bundle_indices) {
        const Bundle& bundle = bundles[b_idx];
        
        std::vector<SinglePlacement> final_placements;
        std::vector<char> temp_occupied = occupied_mask;
        
        // Пытаемся разместить набор целиком
        bool success = place_shapes_recursive(0, bundle.get_shapes(), temp_occupied, final_placements, g);
        
        if (success) {
            // Если удалось, сохраняем результат
            for(const auto& p : final_placements) {
                for(int f_id : p.footprint) {
                    occupied_mask[f_id] = 1;
                    state.node_allocations[f_id] = bundle.get_id();
                    state.node_figure_ids[f_id] = fig_uid_counter;
                }
                fig_uid_counter++;
            }
            state.placed_bundle_ids.push_back(bundle.get_id());
            current_score += (float)bundle.get_total_area();
        }
    }
    
    state.score = current_score;
    return state;
}

SolverResult GRASPSolver::solve() {
    auto start_time = std::chrono::high_resolution_clock::now();
    bool use_timer = (config.max_time_seconds > 0.001);

    // Сброс статистики (не используется)
    
    float best_score = -1.0f;
    SolutionState best_state;
    // Инициализируем пустыми значениями
    best_state.score = -1.0f;
    
    if (config.verbose) {
        std::cout << "GRASP: Запуск оптимизации..." << std::endl;
        if (use_timer) std::cout << "Лимит времени: " << config.max_time_seconds << " сек." << std::endl;
        else std::cout << "Лимит итераций: " << config.max_iterations << std::endl;
    }
    
    int iter = 0;
    while(true) {
        if (use_timer) {
            auto now = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed = now - start_time;
            if (elapsed.count() > config.max_time_seconds) break;
        } else {
            if (iter >= config.max_iterations) break;
        }

        SolutionState current_state = run_construction_phase();
        
        if (current_state.score > best_score) {
            best_score = current_state.score;
            best_state = current_state;
        }
        iter++;
    }
    
    // Применение лучшего найденного результата к сетке
    placed_bundles = best_state.placed_bundle_ids;
    
    // Теперь просто проходим по вектору node_allocations
    if (!best_state.node_allocations.empty()) {
        for(size_t nid = 0; nid < best_state.node_allocations.size(); ++nid) {
            int bid = best_state.node_allocations[nid];
            if (bid != -1) {
                GridCellData& data = graph->get_node(nid).get_data();
                data.bundle_id = bid;
                if (best_state.node_figure_ids[nid] != -1) {
                    data.figure_id = best_state.node_figure_ids[nid];
                }
            }
        }
    }
    
    return { best_score, placed_bundles };
}
