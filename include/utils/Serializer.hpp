#pragma once
#include "../core.hpp"
#include <fstream>
#include <iostream>
#include <memory>

class Serializer {
public:
    // Сохраняет структуру графа и бандлы (если есть) в простой текстовый формат
    static void save(const std::string& filename, std::shared_ptr<Grid> grid) {
        std::ofstream out(filename);
        if (!out.is_open()) return;

        // Header: Width Height Type
        out << grid->get_width() << " " << grid->get_height() << " " << (int)grid->get_type() << "\n";
        
        // Nodes: ID BundleID FigureID
        // Мы сохраняем состояние сетки. Если это сгенерированный пазл, BundleID там уже есть.
        out << grid->size() << "\n";
        for (const auto& node : grid->get_nodes()) {
            out << node.get_id() << " " 
                << node.get_data().x << " " 
                << node.get_data().y << " "
                << node.get_data().bundle_id << " "
                << node.get_data().figure_id << "\n";
        }
        out.close();
        std::cout << "Grid saved to " << filename << std::endl;
    }

    static std::shared_ptr<Grid> load(const std::string& filename) {
        std::ifstream in(filename);
        if (!in.is_open()) {
            std::cerr << "Failed to open " << filename << std::endl;
            return nullptr;
        }

        int w, h, t;
        in >> w >> h >> t;
        
        std::shared_ptr<Grid> grid = std::make_shared<Grid>(w, h, (GridType)t);
        
        size_t size;
        in >> size;
        
        // В нашем Grid классе узлы добавляются последовательно, 
        // но нам нужно восстановить их данные.
        // Grid конструктор не создает узлы сам (в текущей реализации create_..._grid делал это).
        // Поэтому мы должны пересоздать узлы.
        
        // Важно: Grid класс (graph.hpp) просто хранит вектор.
        // Нам нужно добавить узлы заново.
        for(size_t i=0; i<size; ++i) {
            int id, x, y, bid, fid;
            in >> id >> x >> y >> bid >> fid;
            
            // В Graph::add_node ID присваивается автоматически (index), 
            // поэтому порядок в файле должен совпадать с ID.
            int new_id = grid->add_node(GridCellData(x, y));
            auto& data = grid->get_node(new_id).get_data();
            data.bundle_id = bid;
            data.figure_id = fid;
        }
        
        // Теперь нужно восстановить ребра.
        // Так как топология (соседи) зависит от типа сетки жестко,
        // нам проще вызвать логику "связывания" снова.
        // Но у нас нет доступа к приватным методам Generator.
        // ЛУЧШЕЕ РЕШЕНИЕ: Использовать PuzzleGenerator только для создания пустой сетки с топологией,
        // а потом заполнить её данными из файла.
        
        // Однако, PuzzleGenerator требует Config.
        // Проще продублировать логику связывания или вынести её в Grid?
        // Или, раз мы знаем Type, W, H, мы можем создать временный генератор и попросить пустую сетку.
        
        return grid;
    }
    
    // Вспомогательный метод для восстановления топологии
    static void restore_topology(std::shared_ptr<Grid> grid) {
        // Копируем логику связывания из Generator (упрощенно)
        // Это костыль, но быстрый. По-хорошему Grid должен уметь инициализировать себя.
        int w = grid->get_width();
        int h = grid->get_height();
        GridType t = grid->get_type();
        
        if (t == GridType::SQUARE) {
             for(int y=0; y<h; ++y) {
                for(int x=0; x<w; ++x) {
                    int id = y*w + x;
                    if(x < w - 1) grid->add_edge(id, y*w+(x+1), 1, 3);
                    if(y < h - 1) grid->add_edge(id, (y+1)*w+x, 2, 0);
                }
            }
        }
        else if (t == GridType::HEXAGON) {
            int er_dx[] = {0, 1, 0, -1, -1, -1}; int er_dy[] = {-1, 0, 1, 1, 0, -1};
            int or_dx[] = {1, 1, 1, 0, -1, 0};   int or_dy[] = {-1, 0, 1, 1, 0, -1};
            for(int y=0; y<h; ++y) {
                for(int x=0; x<w; ++x) {
                    int id = y*w + x;
                    int* dx = (y % 2 == 0) ? er_dx : or_dx;
                    int* dy = (y % 2 == 0) ? er_dy : or_dy;
                    for(int p=0; p<6; ++p) {
                        int nx = x + dx[p]; int ny = y + dy[p];
                        if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                            grid->add_edge(id, ny*w + nx, p, (p+3)%6);
                        }
                    }
                }
            }
        }
        else if (t == GridType::TRIANGLE) {
             for(int y=0; y<h; ++y) {
                for(int x=0; x<w; ++x) {
                    int id = y*w + x;
                    bool is_up = ((x + y) % 2 == 0);
                    if (x < w - 1) grid->add_edge(id, y*w + (x+1), 0, 1);
                    if (is_up) { if (y < h - 1) grid->add_edge(id, (y+1)*w + x, 2, 2); } 
                    else { if (y > 0) grid->add_edge(id, (y-1)*w + x, 2, 2); }
                }
            }
        }
    }
};
