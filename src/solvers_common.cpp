#include "solvers.h"
#include <vector>
#include <memory>
#include <queue>

// --- Helper: Graph Embedding Check ---

// Функция проверки вложения графа фигуры в граф сетки.
// Это "сердце" геометрических проверок всех алгоритмов.
// 
// Аргументы:
// - grid: указатель на сетку (большой граф)
// - anchor_id: ID узла сетки, к которому мы прикладываем "нулевой" узел фигуры (якорь)
// - figure: указатель на фигуру (малый граф)
// - rotation: целочисленный поворот (сдвиг портов). 
//   Например, если rotation=1, то порт 0 фигуры сопоставляется с портом 1 сетки.
//
// Возвращает:
// - std::vector<int>: список ID узлов сетки, которые займет фигура.
// - Пустой вектор, если фигура не влезает (выход за границы, самопересечение топологии).
std::vector<int> get_embedding(std::shared_ptr<Grid> grid, int anchor_id, std::shared_ptr<Figure> figure, int rotation) {
    if (figure->size() == 0) return {};
    
    // Маппинг: Индекс в массиве = ID узла фигуры, Значение = ID узла сетки
    std::vector<int> mapping(figure->size(), -1); 
    mapping[0] = anchor_id;
    
    // Используем BFS (поиск в ширину) для обхода графа фигуры
    // Мы "распространяем" фигуру по сетке от якоря
    std::vector<int> q = {0};
    std::vector<bool> visited(figure->size(), false);
    visited[0] = true;
    
    size_t head = 0;
    while(head < q.size()) {
        int u_fig = q[head++];
        int u_grid = mapping[u_fig]; // Мы уже знаем, где находится этот узел фигуры в сетке
        
        // Проходим по всем портам (соседям) текущего узла фигуры
        const auto& fig_node = figure->get_node(u_fig);
        
        for(int p = 0; p < figure->get_max_ports(); ++p) {
            int v_fig = fig_node.get_neighbor(p);
            if (v_fig == -1) continue; // По этому направлению у фигуры нет соседа
            
            // Если мы уже посетили этот узел фигуры (он уже размещен), 
            // теоретически нужно проверить, что ребро в сетке тоже существует.
            // Но для регулярных сеток без дыр это гарантировано геометрией, если мы пришли сюда по дереву BFS.
            if (visited[v_fig]) {
                continue; 
            }
            
            // Самое важное: Учет поворота.
            // Если у фигуры сосед на порту P, то на сетке мы ищем соседа на порту (P + rotation) % MaxPorts
            int rot_port = (p + rotation) % grid->get_max_ports();
            int v_grid = grid->get_node(u_grid).get_neighbor(rot_port);
            
            // Проверка 1: Выход за границы сетки
            if (v_grid == -1) return {}; 
            
            // Проверка 2: Самопересечение
            // Нельзя, чтобы два разных узла фигуры попали в одну клетку сетки
            for(int m : mapping) if(m == v_grid) return {};
            
            // Успешно нашли место для соседа
            mapping[v_fig] = v_grid;
            visited[v_fig] = true;
            q.push_back(v_fig);
        }
    }
    
    // Если BFS прошел успешно, значит фигура полностью легла на сетку
    return mapping;
}
