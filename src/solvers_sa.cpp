#include "solvers.h"
#include "heuristics.h"
#include <iostream>
#include <algorithm>
#include <vector>
#include <map>
#include <random>
#include <cmath>
#include <set>

// --- Simulated Annealing Implementation (Permutation-Based) ---
// Теперь алгоритм работает так:
// 1. Состояние = упорядоченный список всех бандлов + выбор эвристики для каждого.
// 2. Энергия = - (площадь покрытия), вычисляемая путем жадного размещения по очереди ("Тетрис").
// 3. Шаг (Neighbor) = поменять местами два бандла в очереди ИЛИ сменить эвристику.

float SimulatedAnnealingSolver::evaluate_sequence(const std::vector<Gene>& sequence, std::map<int, std::vector<PlacedShape>>* out_placement) {
    // Вектор занятости (оптимизированный)
    std::vector<char> occupied(graph->size(), 0);
    // Для эвристик нужен set, так как там интерфейс просит set (можно оптимизировать и там, но пока так)
    std::set<int> occupied_set;
    
    float total_area = 0;
    
    if (out_placement) out_placement->clear();

    for(const auto& gene : sequence) {
        int bid = gene.bundle_id;
        const Bundle* bundle = nullptr;
        for(const auto& b : bundles) if(b.get_id() == bid) { bundle = &b; break; }
        if(!bundle) continue;

        std::vector<PlacedShape> current_bundle_shapes;
        std::set<int> temp_occupied_set = occupied_set; // Копия для отката
        bool possible = true;

        for(const auto& shape : bundle->get_shapes()) {
            HeuristicType h_type = static_cast<HeuristicType>(gene.heuristic);
            
            // 1. Получаем кандидатов (используем существующий модуль)
            std::vector<int> candidates = Heuristics::get_candidates(h_type, graph, temp_occupied_set);

            float best_s = -1e9;
            int best_anchor = -1;
            int best_rot = -1;
            std::vector<int> best_fp;

            for(int anchor : candidates) {
                for(int r=0; r<graph->get_max_ports(); ++r) {
                    auto fp = get_embedding(graph, anchor, shape, r);
                    if (fp.empty()) continue; 
                    
                    bool clash = false;
                    for(int nid : fp) {
                        if(occupied[nid]) { clash = true; break; } // Быстрая проверка
                    }
                    
                    if(!clash) {
                        float s = Heuristics::evaluate(h_type, graph, temp_occupied_set, fp);
                        if(s > best_s) {
                            best_s = s;
                            best_anchor = anchor;
                            best_rot = r;
                            best_fp = fp;
                        }
                    }
                }
            }

            if (best_anchor != -1) {
                PlacedShape ps;
                ps.anchor_id = best_anchor;
                ps.rotation = best_rot;
                ps.figure = shape;
                ps.footprint = best_fp;
                current_bundle_shapes.push_back(ps);
                
                for(int nid : best_fp) {
                    temp_occupied_set.insert(nid);
                }
            } else {
                possible = false;
                break;
            }
        }

        if (possible) {
            // Фиксируем размещение
            for(const auto& ps : current_bundle_shapes) {
                for(int nid : ps.footprint) occupied[nid] = 1;
            }
            occupied_set = temp_occupied_set;
            total_area += (float)bundle->get_total_area();
            
            if (out_placement) {
                (*out_placement)[bid] = current_bundle_shapes;
            }
        }
    }
    
    // Энергия должна минимизироваться, поэтому возвращаем отрицательную площадь
    return -total_area;
}

SimulatedAnnealingSolver::State SimulatedAnnealingSolver::get_neighbor(const State& current, std::mt19937& rng) {
    State next = current;
    int n = next.sequence.size();
    if (n < 2) return next;

    std::uniform_real_distribution<> r_dist(0, 1);
    
    // 1. Swap Mutation (70% chance)
    if (r_dist(rng) < 0.7) {
        std::uniform_int_distribution<> idx_dist(0, n-1);
        int i = idx_dist(rng);
        int j = idx_dist(rng);
        std::swap(next.sequence[i], next.sequence[j]);
    } 
    // 2. Heuristic Mutation (30% chance)
    else {
        std::uniform_int_distribution<> idx_dist(0, n-1);
        std::uniform_int_distribution<> h_dist(0, HEURISTIC_COUNT - 1);
        int i = idx_dist(rng);
        next.sequence[i].heuristic = h_dist(rng);
    }
    
    next.energy = evaluate_sequence(next.sequence);
    return next;
}

float SimulatedAnnealingSolver::solve() {
    int max_iterations = 50; // МИКРО-ТЕСТ
    float initial_temp = 5000.0f;
    float cooling_rate = 0.95f; 

    std::cout << "SA (Permutation): Starting optimization (" << max_iterations << " iters)..." << std::endl;
    std::random_device rd;
    std::mt19937 rng(rd());
    
    // Инициализация случайной последовательности
    State current;
    std::vector<int> bids;
    for(const auto& b : bundles) bids.push_back(b.get_id());
    std::shuffle(bids.begin(), bids.end(), rng);
    
    std::uniform_int_distribution<> h_dist(0, HEURISTIC_COUNT - 1);
    for(int bid : bids) {
        current.sequence.push_back({bid, h_dist(rng)});
    }
    
    current.energy = evaluate_sequence(current.sequence);
    State best = current;
    
    float T = initial_temp;
    
    int stagnation_counter = 0;
    float last_best_energy = best.energy;
    float target_energy = -(float)graph->size();

    for(int i=0; i<max_iterations; ++i) {
        // ВЫВОД КАЖДОЙ ИТЕРАЦИИ
        std::cout << "SA Iter " << i << "/" << max_iterations 
                  << " | T=" << (int)T 
                  << " | Score=" << -best.energy << "\n" << std::flush;

        if (best.energy <= target_energy) {
            std::cout << "\nSA: Perfect solution found!" << std::endl;
            break;
        }

        State neighbor = get_neighbor(current, rng);
        
        float delta = neighbor.energy - current.energy;
        
        if (delta < 0 || std::exp(-delta / T) > std::uniform_real_distribution<>(0, 1)(rng)) {
            current = neighbor;
            if (current.energy < best.energy) {
                best = current;
                stagnation_counter = 0;
            }
        }
        
        T *= cooling_rate;
    }
    std::cout << "\nSA Finished." << std::endl;
    
    std::map<int, std::vector<PlacedShape>> final_placement;
    evaluate_sequence(best.sequence, &final_placement);
    
    int fig_uid = 0;
    for(const auto& [bid, shapes] : final_placement) {
        for(const auto& s : shapes) {
            for(int nid : s.footprint) {
                auto& data = graph->get_node(nid).get_data();
                data.bundle_id = bid;
                data.figure_id = fig_uid;
            }
            fig_uid++;
        }
    }
    
    return -best.energy;
}
