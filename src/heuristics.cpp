#include "heuristics.h"
#include <cmath>
#include <algorithm>
#include <iostream>

float Heuristics::evaluate(HeuristicType type, const std::shared_ptr<Grid>& grid, const std::vector<char>& occupied_mask, const std::vector<int>& footprint) {
    if (footprint.empty()) return -1e9; // Невалидная позиция

    switch (type) {
        case MAX_CONTACT: {
            int contact = 0;
            for(int nid : footprint) {
                const auto& node = grid->get_node(nid);
                for(int n : node.get_all_neighbors()) {
                    if(n != -1 && occupied_mask[n]) contact++;
                }
            }
            return (float)contact;
        }
        
        case BOTTOM_LEFT: {
            double sum_idx = 0;
            for(int nid : footprint) sum_idx += nid;
            return -(float)(sum_idx / footprint.size()); 
        }

        case MIN_HOLES: {
            int contact = 0;
            int free_neighbors = 0;
            for(int nid : footprint) {
                const auto& node = grid->get_node(nid);
                for(int n : node.get_all_neighbors()) {
                    if (n == -1) continue;
                    if (occupied_mask[n]) {
                        contact++;
                    } else {
                        bool is_self = false;
                        for(int f_nid : footprint) if(f_nid == n) { is_self = true; break; }
                        if (!is_self) free_neighbors++;
                    }
                }
            }
            return (float)(contact * 3 - free_neighbors);
        }

        case WALL_HUGGING: {
            // Идея: Приоритет клеткам, которые ближе к границам поля.
            // Сначала заполняем периметр, потом идем внутрь.
            // Score = -MinDistToWall (отрицательная дистанция, чтобы максимум был на стене = 0)
            
            float total_dist = 0;
            int w = grid->get_width();
            int h = grid->get_height();

            for(int nid : footprint) {
                const auto& data = grid->get_node(nid).get_data();
                int x = data.x;
                int y = data.y;
                
                // Дистанция до ближайшей стены
                // Для гексагонов/треугольников это работает как bounding box, что тоже неплохо
                int dist = std::min({x, y, w - 1 - x, h - 1 - y});
                total_dist += dist;
            }
            
            // Чем меньше дистанция до стены, тем лучше.
            return -total_dist; 
        }

        default: return 0.0f;
    }
}

std::vector<int> Heuristics::get_candidates(HeuristicType type, const std::shared_ptr<Grid>& grid, const std::vector<char>& occupied_mask) {
    std::vector<int> candidates;
    
    // Проверка на пустоту
    bool is_empty = true;
    for(char c : occupied_mask) {
        if(c) { is_empty = false; break; }
    }

    if (is_empty) {
        // Если поле пустое, WALL_HUGGING и BOTTOM_LEFT начинают с угла (0,0)
        if (type == BOTTOM_LEFT || type == WALL_HUGGING) candidates.push_back(0);
        // Остальные с центра
        else candidates.push_back((grid->get_height()/2) * grid->get_width() + (grid->get_width()/2));
        return candidates;
    }

    // Для большинства стратегий (MAX_CONTACT, MIN_HOLES, WALL_HUGGING) 
    // имеет смысл проверять только клетки, соседние с уже занятыми.
    if (type == MAX_CONTACT || type == MIN_HOLES || type == WALL_HUGGING) {
        std::vector<char> added_candidate(grid->size(), 0);

        for(int i=0; i < (int)occupied_mask.size(); ++i) {
            if (occupied_mask[i]) {
                const auto& node = grid->get_node(i);
                for(int n : node.get_all_neighbors()) {
                    if(n != -1 && !occupied_mask[n] && !added_candidate[n]) {
                        candidates.push_back(n);
                        added_candidate[n] = 1;
                    }
                }
            }
        }
        return candidates;
    }

    // Для BOTTOM_LEFT ("Тетрис") проверяем первые N свободных клеток
    if (type == BOTTOM_LEFT) {
        int limit = 50; 
        for(int i=0; i < grid->size(); ++i) {
            if (!occupied_mask[i]) {
                candidates.push_back(i);
                if (--limit <= 0) break;
            }
        }
        return candidates;
    }
    
    return candidates;
}
