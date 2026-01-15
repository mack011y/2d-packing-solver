#include "solvers.h"
#include <iostream>
#include <algorithm>
#include <random>
#include <chrono>
#include <set>
#include <map>
#include <queue>

// --- Helper: Graph Embedding Check ---

// Пытается наложить граф фигуры (figure) на граф сетки (grid),
// начиная совмещение figure->node(0) с grid->node(anchor_id).
// rotation — это циклический сдвиг портов.
// Возвращает список занятых узлов сетки или пустой список при неудаче.
std::vector<int> get_embedding(std::shared_ptr<Grid> grid, int anchor_id, std::shared_ptr<Figure> figure, int rotation) {
    if (figure->size() == 0) return {};
    
    std::vector<int> mapping(figure->size(), -1); // FigureNodeID -> GridNodeID
    mapping[0] = anchor_id;
    
    // BFS по графу ФИГУРЫ
    std::vector<int> q = {0};
    std::vector<bool> visited(figure->size(), false);
    visited[0] = true;
    
    size_t head = 0;
    while(head < q.size()) {
        int u_fig = q[head++];
        int u_grid = mapping[u_fig]; // Соответствующий узел в сетке
        
        // Проходим по соседям в фигуре
        const auto& fig_node = figure->get_node(u_fig);
        
        for(int p = 0; p < figure->get_max_ports(); ++p) {
            int v_fig = fig_node.get_neighbor(p);
            if (v_fig == -1) continue; // Нет ребра в фигуре
            
            // Если мы уже "посетили" этот узел фигуры (он уже замаплен), проверяем согласованность
            if (visited[v_fig]) {
                // Проверка цикла: если в фигуре есть ребро u->v, 
                // то и в сетке должно быть ребро u_grid->v_grid (с учетом поворота)
                // Но так как Grid регулярный, достаточно просто проверить, что маппинг не противоречит
                // (тут можно добавить строгую проверку топологии, но для замощения достаточно древесного обхода)
                continue; 
            }
            
            // Ищем соответствующего соседа в сетке с учетом поворота
            int rot_port = (p + rotation) % grid->get_max_ports();
            int v_grid = grid->get_node(u_grid).get_neighbor(rot_port);
            
            if (v_grid == -1) return {}; // Вышли за границы сетки
            
            // Проверка на самопересечение (один узел сетки не может использоваться дважды одной фигурой)
            for(int m : mapping) if(m == v_grid) return {};
            
            mapping[v_fig] = v_grid;
            visited[v_fig] = true;
            q.push_back(v_fig);
        }
    }
    
    // Возвращаем занятые узлы
    return mapping;
}


// --- DLX Solver Implementation ---

float DLXSolver::solve() {
    rows.clear();
    cols.clear();
    placed_bundles.clear();

    std::vector<std::shared_ptr<Figure>> all_shapes;
    for(const auto& b : bundles) {
        for(const auto& s : b.get_shapes()) all_shapes.push_back(s);
    }
    
    for(const auto& s : all_shapes) cols["S:" + s->name] = {};
    for(const auto& node : graph->get_nodes()) cols["N:" + std::to_string(node.get_id())] = {};

    int row_counter = 0;
    
    for(const auto& bundle : bundles) {
        for(const auto& shape : bundle.get_shapes()) {
            std::string shape_col = "S:" + shape->name;
            
            for(const auto& node : graph->get_nodes()) {
                int node_id = node.get_id();
                // Пробуем все повороты
                for(int rot = 0; rot < graph->get_max_ports(); ++rot) {
                    auto fp = get_embedding(graph, node_id, shape, rot);
                    if (fp.empty()) continue;
                    
                    bool valid = true;
                    std::set<std::string> current_cols;
                    current_cols.insert(shape_col);
                    
                    for(int nid : fp) {
                        std::string ncol = "N:" + std::to_string(nid);
                        if (cols.find(ncol) == cols.end()) { valid = false; break; }
                        current_cols.insert(ncol);
                    }
                    if (!valid) continue;
                    
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
    
    for(const auto& s : all_shapes) {
        if(cols["S:" + s->name].empty()) {
            return 0.0f;
        }
    }
    
    std::vector<int> solution;
    if (search(solution)) {
        apply_solution(solution);
        float total_val = 0;
        for(const auto& b : bundles) total_val += (float)b.get_total_area();
        return total_val;
    }
    
    return 0.0f;
}

bool DLXSolver::search(std::vector<int>& solution) {
    if (cols.empty()) return true;
    
    std::string best_col;
    size_t min_len = 999999;
    
    for(const auto& [c, r_set] : cols) {
        if (r_set.size() < min_len) {
            min_len = r_set.size();
            best_col = c;
            if (min_len <= 1) break;
        }
    }
    
    if (min_len == 0) return false;
    
    std::vector<int> candidates(cols[best_col].begin(), cols[best_col].end());
    
    for(int row_id : candidates) {
        solution.push_back(row_id);
        
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


// --- GRASP Solver Implementation ---

struct SinglePlacement {
    std::shared_ptr<Figure> figure;
    int anchor;
    int rotation;
    std::vector<int> footprint;
    int score; 
};

int calculate_placement_score(const std::shared_ptr<Grid>& grid, const std::vector<int>& footprint, const std::set<int>& occupied_nodes) {
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

bool place_shapes_recursive(
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

    int max_score = -9999;
    for(const auto& c : candidates) if(c.score > max_score) max_score = c.score;
    
    std::vector<SinglePlacement> rcl;
    for(const auto& c : candidates) {
        if (c.score >= max_score * alpha || (max_score <= 0)) {
            rcl.push_back(c);
        }
    }
    
    std::shuffle(rcl.begin(), rcl.end(), rng);
    int max_tries = std::min((int)rcl.size(), 5); 
    
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
        
        bool success = place_shapes_recursive(0, bundle.get_shapes(), graph, temp_occupied, final_placements, g, alpha);
        
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
    
    std::cout << "GRASP: Starting " << max_iterations << " iterations..." << std::endl;
    
    for(int i=0; i<max_iterations; ++i) {
        SolutionState current_state;
        float score = run_construction_phase(current_state);
        
        if (score > best_score) {
            best_score = score;
            best_state = current_state;
        }
    }
    
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
