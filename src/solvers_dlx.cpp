#include "solvers.h"
#include <iostream>
#include <algorithm>
#include <set>
#include <map>
#include <string>
#include <vector>

// --- DLX Solver Implementation (Dancing Links / Algorithm X) ---
// Этот солвер использует точный алгоритм покрытия множества (Exact Cover).
// Он строит огромную матрицу, где строки - это возможные размещения фигур,
// а столбцы - это условия (каждая клетка должна быть покрыта ровно один раз).
// ПРЕДУПРЕЖДЕНИЕ: Работает только на ОЧЕНЬ маленьких задачах из-за комбинаторного взрыва.

float DLXSolver::solve() {
    rows.clear();
    cols.clear();
    placed_bundles.clear();

    // 1. Собираем все фигуры из всех бандлов
    std::vector<std::shared_ptr<Figure>> all_shapes;
    for(const auto& b : bundles) {
        for(const auto& s : b.get_shapes()) all_shapes.push_back(s);
    }
    
    // 2. Инициализируем столбцы матрицы
    // Условие 1: Каждая фигура должна быть использована 1 раз (S:Name)
    for(const auto& s : all_shapes) cols["S:" + s->name] = {};
    // Условие 2: Каждая клетка сетки должна быть занята 1 раз (N:ID)
    for(const auto& node : graph->get_nodes()) cols["N:" + std::to_string(node.get_id())] = {};

    int row_counter = 0;
    
    // 3. Генерируем строки матрицы (все возможные легальные размещения)
    for(const auto& bundle : bundles) {
        for(const auto& shape : bundle.get_shapes()) {
            std::string shape_col = "S:" + shape->name;
            
            for(const auto& node : graph->get_nodes()) {
                int node_id = node.get_id();
                // Пробуем все повороты
                for(int rot = 0; rot < graph->get_max_ports(); ++rot) {
                    auto fp = get_embedding(graph, node_id, shape, rot);
                    if (fp.empty()) continue; // Не влезает
                    
                    bool valid = true;
                    std::set<std::string> current_cols;
                    current_cols.insert(shape_col);
                    
                    // Проверяем, какие клетки накрывает
                    for(int nid : fp) {
                        std::string ncol = "N:" + std::to_string(nid);
                        if (cols.find(ncol) == cols.end()) { valid = false; break; } // Неизвестная клетка? (вряд ли)
                        current_cols.insert(ncol);
                    }
                    if (!valid) continue;
                    
                    // Сохраняем строку
                    MatrixRow row;
                    row.id = row_counter;
                    row.cols = current_cols;
                    row.figure = shape;
                    row.anchor_grid_id = node_id;
                    row.rotation = rot;
                    row.bundle_id = bundle.get_id();
                    
                    rows[row_counter] = row;
                    for(const auto& c : current_cols) cols[c].insert(row_counter);
                    
                    row_counter++;
                }
            }
        }
    }
    
    // Быстрая проверка: если для какой-то фигуры нет ни одного варианта размещения, решения нет
    for(const auto& s : all_shapes) {
        if(cols["S:" + s->name].empty()) {
            return 0.0f;
        }
    }
    
    // 4. Запускаем рекурсивный поиск (Algorithm X)
    std::vector<int> solution;
    if (search(solution)) {
        apply_solution(solution);
        float total_val = 0;
        for(const auto& b : bundles) total_val += (float)b.get_total_area();
        return total_val;
    }
    
    return 0.0f;
}

// Рекурсивный поиск с возвратом (Backtracking)
bool DLXSolver::search(std::vector<int>& solution) {
    if (cols.empty()) return true; // Матрица пуста - все условия выполнены!
    
    // Эвристика: выбираем столбец с минимальным количеством вариантов (строк)
    std::string best_col;
    size_t min_len = 999999;
    
    for(const auto& [c, r_set] : cols) {
        if (r_set.size() < min_len) {
            min_len = r_set.size();
            best_col = c;
            if (min_len <= 1) break; // Оптимизация
        }
    }
    
    if (min_len == 0) return false; // Тупик (столбец есть, а покрыть нечем)
    
    std::vector<int> candidates(cols[best_col].begin(), cols[best_col].end());
    
    for(int row_id : candidates) {
        solution.push_back(row_id);
        
        // "Cover": Удаляем выбранную строку и все столбцы, которые она покрывает,
        // а также все другие строки, которые конфликтуют по этим столбцам.
        struct RemovedInfo {
            std::string col;
            std::set<int> rows;
        };
        std::vector<RemovedInfo> removed_cols_history;
        
        auto& row_cols = rows[row_id].cols;
        
        for(const auto& c : row_cols) {
            if (cols.find(c) == cols.end()) continue;
            
            for(int other_row : cols[c]) {
                if (other_row == row_id) continue;
                for(const auto& oc : rows[other_row].cols) {
                    if (oc != c && cols.count(oc)) {
                        cols[oc].erase(other_row);
                    }
                }
            }
            
            removed_cols_history.push_back({c, cols[c]});
            cols.erase(c);
        }
        
        if (search(solution)) return true;
        
        // Backtrack ("Uncover"): восстанавливаем состояние
        solution.pop_back();
        
        for(int i = removed_cols_history.size() - 1; i >= 0; --i) {
            std::string c = removed_cols_history[i].col;
            cols[c] = removed_cols_history[i].rows;
            
            for(int other_row : cols[c]) {
                if (other_row == row_id) continue;
                for(const auto& oc : rows[other_row].cols) {
                    if (oc != c && cols.count(oc)) {
                        cols[oc].insert(other_row);
                    }
                }
            }
        }
    }
    
    return false;
}

void DLXSolver::apply_solution(const std::vector<int>& solution) {
    std::set<int> unique_bundles;
    int fig_uid = 0;
    for(int rid : solution) {
        const auto& r = rows[rid];
        unique_bundles.insert(r.bundle_id);
        
        auto fp = get_embedding(graph, r.anchor_grid_id, r.figure, r.rotation);
        for(int nid : fp) {
             GridCellData& cell = graph->get_node(nid).get_data();
             cell.bundle_id = r.bundle_id;
             cell.figure_id = fig_uid;
        }
        fig_uid++;
    }
    placed_bundles.assign(unique_bundles.begin(), unique_bundles.end());
}
