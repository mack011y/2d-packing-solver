#include "solvers.h"
#include <iostream>
#include <algorithm>
#include <vector>
#include <map>
#include <set>
#include <random>

// --- GRASP Solver Implementation ---
// Greedy Randomized Adaptive Search Procedure
// Этот метод строит решение итеративно, на каждом шаге выбирая "хорошее" место для следующей фигуры,
// но добавляя элемент случайности (RCL - Restricted Candidate List), чтобы избегать локальных минимумов.

struct SinglePlacement {
    std::shared_ptr<Figure> figure;
    int anchor;
    int rotation;
    std::vector<int> footprint;
    int score; 
};

// Функция оценки качества размещения
// Сейчас: бонус за соседство с уже занятыми клетками (плотность)
static int calculate_placement_score(const std::shared_ptr<Grid>& grid, const std::vector<int>& footprint, const std::set<int>& occupied_nodes) {
    int neighbors = 0;
    for (int nid : footprint) {
        const auto& node = grid->get_node(nid);
        for(int neighbor_id : node.get_all_neighbors()) {
            if (neighbor_id != -1 && occupied_nodes.count(neighbor_id)) {
                neighbors += 10;
            }
        }
    }
    return neighbors;
}

// Рекурсивная функция размещения фигур (backtracking с ограничением глубины/жадности)
static bool place_shapes_recursive(
    int shape_idx, 
    const std::vector<std::shared_ptr<Figure>>& shapes, 
    std::shared_ptr<Grid> grid, 
    std::set<int>& current_occupied, 
    std::vector<SinglePlacement>& out_placements,
    std::mt19937& rng,
    float alpha
) {
    if (shape_idx >= (int)shapes.size()) return true; 

    const auto& shape = shapes[shape_idx];
    std::vector<SinglePlacement> candidates;
    
    // 1. Поиск всех возможных мест для текущей фигуры
    for(const auto& node : grid->get_nodes()) {
        int nid = node.get_id();
        if (current_occupied.count(nid)) continue;

        for(int rot=0; rot < grid->get_max_ports(); ++rot) {
            auto fp = get_embedding(grid, nid, shape, rot);
            if (fp.empty()) continue;
            
            bool clash = false;
            for(int f_id : fp) {
                if (current_occupied.count(f_id)) { clash = true; break; }
            }
            if (!clash) {
                int score = calculate_placement_score(grid, fp, current_occupied);
                candidates.push_back({shape, nid, rot, fp, score});
            }
        }
    }
    
    if (candidates.empty()) return false;

    // 2. Построение RCL (Restricted Candidate List)
    int max_score = -9999;
    for(const auto& c : candidates) if(c.score > max_score) max_score = c.score;
    
    std::vector<SinglePlacement> rcl;
    for(const auto& c : candidates) {
        // Берем кандидатов, которые не хуже alpha * max_score
        if (c.score >= max_score * alpha || (max_score <= 0)) {
            rcl.push_back(c);
        }
    }
    
    // Случайный выбор из лучших
    std::shuffle(rcl.begin(), rcl.end(), rng);
    int max_tries = std::min((int)rcl.size(), 5); // Ограничиваем ветвление
    
    for(int i=0; i<max_tries; ++i) {
        const auto& choice = rcl[i];
        
        std::set<int> new_occupied = current_occupied;
        for(int fid : choice.footprint) new_occupied.insert(fid);
        
        out_placements.push_back(choice);
        
        if (place_shapes_recursive(shape_idx + 1, shapes, grid, new_occupied, out_placements, rng, alpha)) {
            current_occupied = new_occupied;
            return true;
        }
        
        out_placements.pop_back();
    }
    
    return false;
}

float GRASPSolver::run_construction_phase(SolutionState& state) {
    // Сортировка бандлов: сначала большие и сложные
    std::vector<int> bundle_indices(bundles.size());
    for(size_t i=0; i<bundles.size(); ++i) bundle_indices[i] = i;
    
    std::sort(bundle_indices.begin(), bundle_indices.end(), [&](int a, int b){
        if (bundles[a].get_total_area() != bundles[b].get_total_area())
            return bundles[a].get_total_area() > bundles[b].get_total_area();
        return bundles[a].get_shapes().size() > bundles[b].get_shapes().size();
    });

    float current_score = 0.0f;
    state.placed_bundle_ids.clear();
    state.node_allocations.clear();
    state.node_figure_ids.clear();
    
    std::set<int> occupied_nodes;
    static std::random_device rd;
    static std::mt19937 g(rd());

    int fig_uid_counter = 0;

    for(int b_idx : bundle_indices) {
        const auto& bundle = bundles[b_idx];
        
        std::vector<SinglePlacement> final_placements;
        std::set<int> temp_occupied = occupied_nodes;
        
        // Пытаемся разместить бандл целиком
        bool success = place_shapes_recursive(0, bundle.get_shapes(), graph, temp_occupied, final_placements, g, config.grasp_alpha);
        
        if (success) {
            for(const auto& p : final_placements) {
                for(int f_id : p.footprint) {
                    occupied_nodes.insert(f_id);
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
    return current_score;
}

float GRASPSolver::solve() {
    float best_score = -1.0f;
    SolutionState best_state;
    
    if (config.verbose) {
        std::cout << "GRASP: Starting " << config.grasp_max_iterations << " iterations..." << std::endl;
    }
    
    // Многократный запуск фазы построения
    for(int i=0; i<config.grasp_max_iterations; ++i) {
        SolutionState current_state;
        float score = run_construction_phase(current_state);
        
        if (score > best_score) {
            best_score = score;
            best_state = current_state;
        }
    }
    
    // Применение лучшего результата
    placed_bundles = best_state.placed_bundle_ids;
    for(auto const& [nid, bid] : best_state.node_allocations) {
        if (nid >= 0 && nid < (int)graph->size()) {
            auto& data = graph->get_node(nid).get_data();
            data.bundle_id = bid;
            if (best_state.node_figure_ids.count(nid)) {
                data.figure_id = best_state.node_figure_ids.at(nid);
            }
        }
    }
    
    return best_score;
}
