#include "solvers.h"
#include <iostream>
#include <algorithm>
#include <random>
#include <set>
#include <map>
#include <vector>

// --- Genetic Algorithm Solver Implementation (Classic) ---
// Оптимизированная версия с использованием O(1) проверки коллизий.

// Расчет фитнеса (просто площадь)
float GeneticAlgorithmSolver::calculate_fitness(const Individual& ind) {
    float area = 0;
    for(const auto& [bid, shapes] : ind.active_bundles) {
        for(const auto& s : shapes) {
            area += (float)s.footprint.size();
        }
    }
    return area;
}

// Попытка добавить бандл в особь с использованием "Sticky Heuristic"
// Эта функция пытается найти место для всех фигур бандла так, чтобы они примыкали к существующим.
bool GeneticAlgorithmSolver::try_add_bundle(Individual& ind, int bundle_id, std::mt19937& rng) {
    const Bundle* target_bundle = nullptr;
    for(const auto& b : bundles) if(b.get_id() == bundle_id) { target_bundle = &b; break; }
    if (!target_bundle) return false;

    // Составляем список кандидатов (якорей):
    std::vector<int> potential_anchors;
    
    // Оптимизированная проверка на пустоту (перебор вектора O(N), но это один раз)
    // Можно хранить счетчик занятых узлов в Individual, но пока так:
    bool is_empty = true;
    for(char occupied : ind.occupied_nodes) {
        if(occupied) { is_empty = false; break; }
    }

    if (is_empty) {
        std::uniform_int_distribution<> d(0, graph->size()-1);
        potential_anchors.push_back(d(rng));
    } else {
        // Ищем соседей занятых клеток
        // O(N) проход вместо итерации по сету, но зато без аллокаций
        std::vector<int> neighbors;
        neighbors.reserve(graph->size() / 10); 

        // Чтобы не сканировать весь вектор, можно было бы хранить список занятых ID отдельно,
        // но для простоты пройдемся по тем фигурам, что уже есть
        std::set<int> visited_neighbors; // Чтобы избежать дублей

        for(const auto& [bid, shapes] : ind.active_bundles) {
            for(const auto& shape : shapes) {
                for(int nid : shape.footprint) {
                    const auto& node = graph->get_node(nid);
                    for(int neighbor_id : node.get_all_neighbors()) {
                        if(neighbor_id != -1 && !ind.occupied_nodes[neighbor_id] && visited_neighbors.find(neighbor_id) == visited_neighbors.end()) {
                            neighbors.push_back(neighbor_id);
                            visited_neighbors.insert(neighbor_id);
                        }
                    }
                }
            }
        }
        
        if (neighbors.empty()) {
             // Если вдруг все зажато или ошибка - пробуем рандом
             std::uniform_int_distribution<> d(0, graph->size()-1);
             potential_anchors.push_back(d(rng));
        } else {
            potential_anchors = std::move(neighbors);
            std::shuffle(potential_anchors.begin(), potential_anchors.end(), rng);
             // Добавляем немного шума
            std::uniform_int_distribution<> d(0, graph->size()-1);
            for(int i=0; i<3; ++i) potential_anchors.push_back(d(rng));
        }
    }
    
    if(potential_anchors.size() > 50) potential_anchors.resize(50);

    // Пытаемся разместить
    std::vector<PlacedShape> new_shapes;
    std::vector<char> temp_occupied = ind.occupied_nodes; // Копия вектора (быстро для char)

    for(int anchor_candidate : potential_anchors) {
        new_shapes.clear();
        bool possible = true;
        temp_occupied = ind.occupied_nodes; // Reset

        for(const auto& shape : target_bundle->get_shapes()) {
            std::uniform_int_distribution<> rot_dist(0, graph->get_max_ports()-1);
            
            bool placed_this = false;
            // Пробуем все повороты
            for(int r=0; r<graph->get_max_ports(); ++r) {
                int current_anchor = (new_shapes.empty()) ? anchor_candidate : -1;
                
                // Если это не первая фигура бандла, ищем место рядом с только что поставленными
                if (current_anchor == -1) {
                    std::vector<int> local_anchors;
                    for(const auto& ps : new_shapes) {
                        for(int nid : ps.footprint) {
                             const auto& node = graph->get_node(nid);
                             for(int n : node.get_all_neighbors()) {
                                 if(n!=-1 && !temp_occupied[n]) { // O(1) check
                                     local_anchors.push_back(n);
                                 }
                             }
                        }
                    }
                     if(!local_anchors.empty()) {
                         std::uniform_int_distribution<> d(0, local_anchors.size()-1);
                         current_anchor = local_anchors[d(rng)];
                     } else {
                         std::uniform_int_distribution<> d(0, graph->size()-1);
                         current_anchor = d(rng);
                     }
                }
                
                int rot = (r + rot_dist(rng)) % graph->get_max_ports();
                auto fp = get_embedding(graph, current_anchor, shape, rot);
                
                if (fp.empty()) continue;

                bool collision = false;
                // FAST COLLISION CHECK O(K) where K is shape size
                for(int nid : fp) {
                    if(temp_occupied[nid]) { collision = true; break; }
                }
                
                if (!collision) {
                    PlacedShape ps;
                    ps.anchor_id = current_anchor;
                    ps.rotation = rot;
                    ps.figure = shape;
                    ps.footprint = fp;
                    new_shapes.push_back(ps);
                    for(int nid : fp) temp_occupied[nid] = 1; // Mark as occupied
                    placed_this = true;
                    break;
                }
            }
            
            if (!placed_this) {
                possible = false;
                break;
            }
        }

        if (possible) {
            ind.active_bundles[bundle_id] = new_shapes;
            ind.occupied_nodes = temp_occupied;
            return true;
        }
    }
    
    return false;
}

// Создание случайной особи
GeneticAlgorithmSolver::Individual GeneticAlgorithmSolver::create_random_individual(std::mt19937& rng) {
    Individual ind;
    ind.fitness = 0;
    // Init vector with zeros
    ind.occupied_nodes.resize(graph->size(), 0);
    
    std::vector<int> all_bundle_ids;
    for(const auto& b : bundles) all_bundle_ids.push_back(b.get_id());
    std::shuffle(all_bundle_ids.begin(), all_bundle_ids.end(), rng);

    for(int bid : all_bundle_ids) {
        try_add_bundle(ind, bid, rng);
    }
    ind.fitness = calculate_fitness(ind);
    return ind;
}

// Скрещивание
GeneticAlgorithmSolver::Individual GeneticAlgorithmSolver::crossover(const Individual& p1, const Individual& p2, std::mt19937& rng) {
    Individual child;
    child.fitness = 0;
    child.occupied_nodes.resize(graph->size(), 0);
    
    std::vector<int> p1_bids;
    for(const auto& [bid, _] : p1.active_bundles) p1_bids.push_back(bid);
    
    std::vector<int> p2_bids;
    for(const auto& [bid, _] : p2.active_bundles) p2_bids.push_back(bid);
    
    std::set<int> child_bids;
    
    // Наследуем от P1 (с вероятностью 50%)
    for(int bid : p1_bids) {
        if(std::uniform_real_distribution<>(0, 1)(rng) < 0.5f) {
            const auto& shapes = p1.active_bundles.at(bid);
            bool clash = false;
            for(const auto& s : shapes) {
                for(int nid : s.footprint) if(child.occupied_nodes[nid]) clash = true;
            }
            if(!clash) {
                child.active_bundles[bid] = shapes;
                for(const auto& s : shapes) for(int nid : s.footprint) child.occupied_nodes[nid] = 1;
                child_bids.insert(bid);
            }
        }
    }
    
    // Наследуем от P2
    for(int bid : p2_bids) {
        if (child_bids.count(bid)) continue; 
        
        const auto& shapes = p2.active_bundles.at(bid);
        bool clash = false;
        for(const auto& s : shapes) {
            for(int nid : s.footprint) if(child.occupied_nodes[nid]) clash = true;
        }
        if(!clash) {
            child.active_bundles[bid] = shapes;
            for(const auto& s : shapes) for(int nid : s.footprint) child.occupied_nodes[nid] = 1;
            child_bids.insert(bid);
        }
    }
    
    // Заполняем пустоты
    std::vector<int> all_bids;
    for(const auto& b : bundles) if(!child_bids.count(b.get_id())) all_bids.push_back(b.get_id());
    std::shuffle(all_bids.begin(), all_bids.end(), rng);
    
    for(int bid : all_bids) {
        try_add_bundle(child, bid, rng);
    }
    
    child.fitness = calculate_fitness(child);
    return child;
}

// Мутация
void GeneticAlgorithmSolver::mutate(Individual& ind, std::mt19937& rng) {
    if (!ind.active_bundles.empty()) {
        std::vector<int> bids;
        for(const auto& [bid, _] : ind.active_bundles) bids.push_back(bid);
        std::uniform_int_distribution<> dist(0, bids.size()-1);
        int bid_to_remove = bids[dist(rng)];
        
        const auto& shapes = ind.active_bundles[bid_to_remove];
        for(const auto& s : shapes) {
            for(int nid : s.footprint) ind.occupied_nodes[nid] = 0; // Освобождаем
        }
        ind.active_bundles.erase(bid_to_remove);
    }
    
    std::vector<int> missing_bids;
    for(const auto& b : bundles) {
        if (ind.active_bundles.find(b.get_id()) == ind.active_bundles.end()) {
            missing_bids.push_back(b.get_id());
        }
    }
    
    if (!missing_bids.empty()) {
        std::uniform_int_distribution<> dist(0, missing_bids.size()-1);
        int bid = missing_bids[dist(rng)];
        try_add_bundle(ind, bid, rng);
    }
    
    ind.fitness = calculate_fitness(ind);
}

float GeneticAlgorithmSolver::solve() {
    std::cout << "GA: Starting Genetic Algorithm (" << generations << " gens, pop=" << population_size << ")..." << std::endl;
    
    std::random_device rd;
    std::mt19937 rng(rd());
    
    std::vector<Individual> population;
    for(int i=0; i<population_size; ++i) {
        population.push_back(create_random_individual(rng));
    }
    
    Individual best_ever = population[0];
    
    for(int gen=0; gen<generations; ++gen) {
        std::sort(population.begin(), population.end(), [](const Individual& a, const Individual& b){
            return a.fitness > b.fitness;
        });
        
        if (population[0].fitness > best_ever.fitness) {
            best_ever = population[0];
        }
        
        if (gen % 10 == 0) {
            std::cout << "Gen " << gen << " | Best Fitness: " << best_ever.fitness << "\r" << std::flush;
        }
        
        std::vector<Individual> new_pop;
        
        for(int i=0; i<elite_count && i < population.size(); ++i) {
            new_pop.push_back(population[i]);
        }
        
        while(new_pop.size() < population_size) {
            int t_size = 3;
            Individual p1, p2;
            
            // Tournament selection
            std::uniform_int_distribution<> dist(0, population.size()-1);
            p1 = population[dist(rng)];
            for(int k=0; k<t_size-1; ++k) {
                const auto& other = population[dist(rng)];
                if(other.fitness > p1.fitness) p1 = other;
            }
            
            p2 = population[dist(rng)];
            for(int k=0; k<t_size-1; ++k) {
                const auto& other = population[dist(rng)];
                if(other.fitness > p2.fitness) p2 = other;
            }
            
            Individual child = crossover(p1, p2, rng);
            if (std::uniform_real_distribution<>(0, 1)(rng) < mutation_rate) {
                mutate(child, rng);
            }
            new_pop.push_back(child);
        }
        
        population = new_pop;
    }
    
    std::cout << "\nGA Finished." << std::endl;
    
    int fig_uid = 0;
    for(const auto& [bid, shapes] : best_ever.active_bundles) {
        for(const auto& s : shapes) {
            for(int nid : s.footprint) {
                auto& data = graph->get_node(nid).get_data();
                data.bundle_id = bid;
                data.figure_id = fig_uid;
            }
            fig_uid++;
        }
    }
    
    placed_bundles.clear();
    for(const auto& [bid, _] : best_ever.active_bundles) placed_bundles.push_back(bid);
    
    return best_ever.fitness;
}
