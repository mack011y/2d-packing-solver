#include "heuristics.h"
#include <cmath>
#include <algorithm>

float Heuristics::evaluate(HeuristicType type, const std::shared_ptr<Grid>& grid, const std::set<int>& occupied, const std::vector<int>& footprint) {
    if (footprint.empty()) return -1e9;

    switch (type) {
        case MAX_CONTACT: {
            int contact = 0;
            for(int nid : footprint) {
                const auto& node = grid->get_node(nid);
                for(int n : node.get_all_neighbors()) {
                    if(n != -1 && occupied.count(n)) contact++;
                }
            }
            return (float)contact;
        }
        
        case BOTTOM_LEFT: {
            // BL Score: минимизируем индекс (для стандартной сетки это Y*W + X).
            // Берем средний индекс фигуры
            double sum_idx = 0;
            for(int nid : footprint) sum_idx += nid;
            return -(float)(sum_idx / footprint.size()); 
        }

        case MIN_HOLES: {
            // "Плотность": (Контакты * 2) - (Периметр свободных соседей)
            int contact = 0;
            int free_neighbors = 0;
            
            // Нам нужно быстро проверить, входит ли сосед в саму фигуру (footprint)
            // Для этого временно считаем footprint занятым (или просто проверяем через std::find, что медленно)
            // Оптимизация: footprint мал (3-5 клеток), std::find ok.
            
            for(int nid : footprint) {
                const auto& node = grid->get_node(nid);
                for(int n : node.get_all_neighbors()) {
                    if (n == -1) continue;
                    
                    if (occupied.count(n)) {
                        contact++;
                    } else {
                        // Если сосед свободен, проверяем, не является ли он частью самой фигуры
                        bool is_self = false;
                        for(int f_nid : footprint) if(f_nid == n) { is_self = true; break; }
                        
                        if (!is_self) free_neighbors++;
                    }
                }
            }
            return (float)(contact * 3 - free_neighbors);
        }

        case CENTER_GRAVITY: {
            // Минимизируем расстояние до центра
            double dist_sum = 0;
            double cx = grid->get_width() / 2.0;
            double cy = grid->get_height() / 2.0;

            for(int nid : footprint) {
                const auto& data = grid->get_node(nid).get_data();
                double dx = data.x - cx;
                double dy = data.y - cy;
                dist_sum += std::sqrt(dx*dx + dy*dy);
            }
            return -(float)dist_sum; // Чем меньше дистанция, тем больше score
        }

        default: return 0.0f;
    }
}

std::vector<int> Heuristics::get_candidates(HeuristicType type, const std::shared_ptr<Grid>& grid, const std::set<int>& occupied) {
    std::vector<int> candidates;
    
    // Если поле пустое, все эвристики (кроме BL) начинают с центра или угла
    if (occupied.empty()) {
        if (type == BOTTOM_LEFT) candidates.push_back(0);
        else candidates.push_back((grid->get_height()/2) * grid->get_width() + (grid->get_width()/2));
        return candidates;
    }

    // Для большинства эвристик интересны только "соседи занятых клеток"
    if (type == MAX_CONTACT || type == MIN_HOLES || type == CENTER_GRAVITY) {
        std::set<int> neighbors;
        for(int nid : occupied) {
            const auto& node = grid->get_node(nid);
            for(int n : node.get_all_neighbors()) {
                if(n != -1 && !occupied.count(n)) neighbors.insert(n);
            }
        }
        candidates.assign(neighbors.begin(), neighbors.end());
        return candidates;
    }

    // Для BOTTOM_LEFT - перебор свободных клеток по порядку
    if (type == BOTTOM_LEFT) {
        int limit = 50; // Ограничение для скорости
        for(int i=0; i < grid->size(); ++i) {
            if (!occupied.count(i)) {
                candidates.push_back(i);
                if (--limit <= 0) break;
            }
        }
        return candidates;
    }
    
    return candidates;
}
