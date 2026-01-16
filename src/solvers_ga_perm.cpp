#include "solvers.h"
#include "heuristics.h"
#include <iostream>
#include <algorithm>
#include <random>
#include <set>
#include <map>
#include <vector>

// --- Реализация Генетического Алгоритма с Гипер-Эвристиками ---

GeneticPermutationSolver::PlacementResult GeneticPermutationSolver::build_solution(const std::vector<Gene>& chromosome) {
    PlacementResult result;
    result.score = 0;
    std::set<int> occupied;
    
    for(const auto& gene : chromosome) {
        int bid = gene.bundle_id;
        
        const Bundle* bundle = nullptr;
        for(const auto& b : bundles) if(b.get_id() == bid) { bundle = &b; break; }
        if(!bundle) continue;

        std::vector<PlacedShape> placed_shapes;
        std::set<int> temp_occupied = occupied;
        bool possible = true;

        for(const auto& shape : bundle->get_shapes()) {
            HeuristicType h_type = static_cast<HeuristicType>(gene.heuristic);
            
            // 1. Получаем список кандидатов через модуль эвристик
            std::vector<int> candidates = Heuristics::get_candidates(h_type, graph, temp_occupied);

            // 2. Оценка кандидатов
            float best_score = -1e9;
            int best_anchor = -1;
            int best_rot = -1;
            std::vector<int> best_fp;

            for(int anchor : candidates) {
                for(int r=0; r<graph->get_max_ports(); ++r) {
                    auto fp = get_embedding(graph, anchor, shape, r);
                    if (fp.empty()) continue; 
                    
                    bool clash = false;
                    for(int nid : fp) {
                        if(temp_occupied.count(nid)) { clash = true; break; } 
                    }
                    
                    if(!clash) {
                        // Оценка через модуль эвристик
                        float score = Heuristics::evaluate(h_type, graph, temp_occupied, fp);

                        if(score > best_score) {
                            best_score = score;
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
                placed_shapes.push_back(ps);
                for(int nid : best_fp) temp_occupied.insert(nid);
            } else {
                possible = false;
                break;
            }
        }

        if (possible) {
            result.active_bundles[bid] = placed_shapes;
            occupied = temp_occupied;
            result.score += (float)bundle->get_total_area();
        }
    }
    
    return result;
}

GeneticPermutationSolver::Individual GeneticPermutationSolver::create_random_individual(std::mt19937& rng) {
    Individual ind;
    std::vector<int> bids;
    for(const auto& b : bundles) bids.push_back(b.get_id());
    std::shuffle(bids.begin(), bids.end(), rng);

    std::uniform_int_distribution<> heur_dist(0, HEURISTIC_COUNT - 1);

    for(int bid : bids) {
        Gene g;
        g.bundle_id = bid;
        g.heuristic = heur_dist(rng);
        ind.chromosome.push_back(g);
    }
    
    auto res = build_solution(ind.chromosome);
    ind.fitness = res.score;
    return ind;
}

GeneticPermutationSolver::Individual GeneticPermutationSolver::crossover(const Individual& p1, const Individual& p2, std::mt19937& rng) {
    int n = p1.chromosome.size();
    Individual child;
    child.chromosome.resize(n);
    
    std::uniform_int_distribution<> dist(0, n-1);
    int start = dist(rng);
    int end = dist(rng);
    if(start > end) std::swap(start, end);
    
    std::set<int> copied_bids;
    for(int i=start; i<=end; ++i) {
        child.chromosome[i] = p1.chromosome[i];
        copied_bids.insert(p1.chromosome[i].bundle_id);
    }
    
    int current_p2 = 0;
    for(int i=0; i<n; ++i) {
        if (i >= start && i <= end) continue;
        
        while(copied_bids.count(p2.chromosome[current_p2].bundle_id)) {
            current_p2++;
        }
        child.chromosome[i] = p2.chromosome[current_p2];
        current_p2++;
    }
    
    auto res = build_solution(child.chromosome);
    child.fitness = res.score;
    return child;
}

void GeneticPermutationSolver::mutate(Individual& ind, std::mt19937& rng) {
    int n = ind.chromosome.size();
    if (n < 2) return;

    std::uniform_real_distribution<> rdist(0, 1);
    
    // 1. Scramble Mutation
    if (rdist(rng) < 0.7) {
        std::uniform_int_distribution<> dist(0, n-1);
        int i = dist(rng);
        int j = dist(rng);
        if(i > j) std::swap(i, j);
        if (j - i < 2) { if (j < n-1) j++; else if (i > 0) i--; }
        
        std::shuffle(ind.chromosome.begin() + i, ind.chromosome.begin() + j + 1, rng);
    }
    
    // 2. Heuristic Mutation
    if (rdist(rng) < 0.5) {
        std::uniform_int_distribution<> dist(0, n-1);
        std::uniform_int_distribution<> h_dist(0, HEURISTIC_COUNT - 1);
        
        int i = dist(rng);
        ind.chromosome[i].heuristic = h_dist(rng);
    }

    auto res = build_solution(ind.chromosome);
    ind.fitness = res.score;
}

float GeneticPermutationSolver::solve() {
    std::cout << "GA-Hyper: Starting Hyper-Heuristic GA (" << generations << " gens, pop=" << population_size << ")..." << std::endl;
    
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
        
        if (gen % 1 == 0) {
            std::cout << "Gen " << gen << " | Best Fitness: " << best_ever.fitness << "\r" << std::flush;
        }
        
        std::vector<Individual> new_pop;
        for(int i=0; i<elite_count && i < population.size(); ++i) new_pop.push_back(population[i]);
        
        while(new_pop.size() < population_size) {
            int t_size = 3;
            Individual p1 = population[std::uniform_int_distribution<>(0, population.size()-1)(rng)];
            for(int k=0; k<t_size-1; ++k) {
                auto other = population[std::uniform_int_distribution<>(0, population.size()-1)(rng)];
                if(other.fitness > p1.fitness) p1 = other;
            }
            Individual p2 = population[std::uniform_int_distribution<>(0, population.size()-1)(rng)];
            for(int k=0; k<t_size-1; ++k) {
                auto other = population[std::uniform_int_distribution<>(0, population.size()-1)(rng)];
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
    
    std::cout << "\nGA-Hyper Finished." << std::endl;
    
    auto final_res = build_solution(best_ever.chromosome);
    
    int fig_uid = 0;
    for(const auto& [bid, shapes] : final_res.active_bundles) {
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
    for(const auto& [bid, _] : final_res.active_bundles) placed_bundles.push_back(bid);
    
    return best_ever.fitness;
}
