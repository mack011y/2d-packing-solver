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
// Алгоритм имитации отжига на перестановках.
// 1. Состояние = список бандлов в определенном порядке + выбранная эвристика для каждого.
// 2. Энергия = отрицательная площадь покрытия (чем меньше энергия, тем больше покрыли).
// 3. Шаг = обмен бандлов местами или смена эвристики.

float SimulatedAnnealingSolver::evaluate_sequence(const std::vector<Gene>& sequence, std::map<int, std::vector<PlacedShape>>* out_placement) {
    // Векторная маска занятости (O(1))
    std::vector<char> occupied(graph->size(), 0);
    
    float total_area = 0;
    
    if (out_placement) out_placement->clear();

    for(const auto& gene : sequence) {
        int bid = gene.bundle_id;
        const Bundle* bundle = nullptr;
        for(const auto& b : bundles) if(b.get_id() == bid) { bundle = &b; break; }
        if(!bundle) continue;

        std::vector<PlacedShape> current_bundle_shapes;
        std::vector<char> temp_occupied = occupied; // Копия маски для отката, если бандл не влезет целиком
        bool possible = true;

        for(const auto& shape : bundle->get_shapes()) {
            HeuristicType h_type = static_cast<HeuristicType>(gene.heuristic);
            
            // 1. Получаем кандидатов (теперь передаем векторную маску)
            std::vector<int> candidates = Heuristics::get_candidates(h_type, graph, temp_occupied);

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
                        if(temp_occupied[nid]) { clash = true; break; } // Быстрая проверка
                    }
                    
                    if(!clash) {
                        // Оценка тоже через векторную маску
                        float s = Heuristics::evaluate(h_type, graph, temp_occupied, fp);
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
                
                // Временно занимаем место (обновляем маску)
                for(int nid : best_fp) {
                    temp_occupied[nid] = 1;
                }
            } else {
                possible = false;
                break;
            }
        }

        if (possible) {
            // Если весь бандл влез, фиксируем изменения в основной маске
            occupied = temp_occupied;
            total_area += (float)bundle->get_total_area();
            
            if (out_placement) {
                (*out_placement)[bid] = current_bundle_shapes;
            }
        }
    }
    
    return -total_area;
}

SimulatedAnnealingSolver::State SimulatedAnnealingSolver::get_neighbor(const State& current, std::mt19937& rng) {
    State next = current;
    int n = next.sequence.size();
    if (n < 2) return next;

    std::uniform_real_distribution<> r_dist(0, 1);
    
    // 1. Swap Mutation (70% шанс) - меняем местами два бандла в очереди
    if (r_dist(rng) < 0.7) {
        std::uniform_int_distribution<> idx_dist(0, n-1);
        int i = idx_dist(rng);
        int j = idx_dist(rng);
        std::swap(next.sequence[i], next.sequence[j]);
    } 
    // 2. Heuristic Mutation (30% шанс) - меняем стратегию для одного бандла
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
    if (config.verbose) {
        std::cout << "SA (Permutation): Starting optimization (" << config.sa_max_iterations << " iters)..." << std::endl;
    }
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
    
    float T = config.sa_initial_temp;
    float target_energy = -(float)graph->size();

    // Логирование реже, чтобы не спамить в консоль
    int log_interval = config.sa_max_iterations / 20;
    if (log_interval == 0) log_interval = 1;

    for(int i=0; i<config.sa_max_iterations; ++i) {
        if (config.verbose && i % log_interval == 0) {
            std::cout << "SA Iter " << i << "/" << config.sa_max_iterations 
                      << " | T=" << (int)T 
                      << " | Score=" << -best.energy << "\n" << std::flush;
        }

        if (best.energy <= target_energy) {
            std::cout << "\nSA: Perfect solution found!" << std::endl;
            break;
        }

        State neighbor = get_neighbor(current, rng);
        float delta = neighbor.energy - current.energy;
        
        // Критерий Метрополиса
        if (delta < 0 || std::exp(-delta / T) > std::uniform_real_distribution<>(0, 1)(rng)) {
            current = neighbor;
            if (current.energy < best.energy) {
                best = current;
            }
        }
        
        T *= config.sa_cooling_rate;
    }
    if (config.verbose) {
        std::cout << "\nSA Finished. Best Score: " << -best.energy << std::endl;
    }
    
    // Финализация: применяем лучшее решение к сетке
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
