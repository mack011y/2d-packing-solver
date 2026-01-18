#pragma once
#include <vector>
#include <string>
#include <memory>
#include <map>
#include "graph.hpp"

enum class GridType {
    SQUARE,   // 4 сосед
    HEXAGON,  // 6 соседей
    TRIANGLE  // 3 соседа
};

struct Color {
    int r, g, b;
};

// Данные для узла фигуры (пока пустая структура, потому что фигуры однородные)
struct FigureNodeData {};

// Фигура представляет собой СВЯЗНЫЙ подграф сетки
class Figure : public Graph<FigureNodeData> {
public:
    std::string name; 
    
    Figure(std::string n, int mp) : Graph<FigureNodeData>(mp), name(n) {}
};

// Данные для ячейки поля
struct GridCellData {
    int x, y;        // Координаты
    int bundle_id;   // к какой группе пренадлежит(нужно для отрисовки цвета)
    int figure_id;   // к какой фигуре в группе пренадлежит
    
    GridCellData(int x = 0, int y = 0) : x(x), y(y), bundle_id(-1), figure_id(-1) {}
};

/*
Во всех трех случаях двумерная плосткость может быть представленая в виде 
сетки, в случаях 6 и 3 угольных ячеек -- соседи будут сдвинуты или соседство будет опредеятся не 
стандартным плюсикм(+)
*/
class Grid : public Graph<GridCellData> {
private:
    int width, height; // Размеры
    GridType type;     // Тип

    static size_t get_max_ports_for_type(GridType t) {
        switch (t) {
            case GridType::HEXAGON: return 6;
            case GridType::TRIANGLE: return 3;
            case GridType::SQUARE: default: return 4;
        }
    }

public:
    Grid(int w, int h, GridType t) 
        : Graph<GridCellData>(get_max_ports_for_type(t)),
          width(w), height(h), type(t) {}

    int get_width() const { return width; }
    int get_height() const { return height; }
    GridType get_type() const { return type; }

    // Возвращает ID узла или -1, если координаты выходят за границы.
    int get_node_id_at(int x, int y) const {
        if (x < 0 || x >= width || y < 0 || y >= height) return -1;
        return y * width + x;
    }

    // Проверяет возможность размещения фигуры
    std::vector<int> get_embedding(std::shared_ptr<Figure> figure, int anchor_id, int rotation) const;
};

// Набор фигур (Bundle)
class Bundle {
private:
    int id;                                      
    std::vector<std::shared_ptr<Figure>> shapes; // Список фигур в наборе 
    size_t total_area;                           // количество занимаемых клеток, сумируем по всем фигурам в наборе
    Color color;                                

public:
    Bundle() : id(-1), total_area(0), color{255, 255, 255} {} // Белый вет

    // Конструктор с перемещением вектора фигур (для эффективности)
    Bundle(int id, std::vector<std::shared_ptr<Figure>> shapes, const Color& color);

    // Пересчитывает общую площадь бандла
    void recalculate_area();

    int get_id() const { return id; }
    const std::vector<std::shared_ptr<Figure>>& get_shapes() const { return shapes; }
    size_t get_total_area() const { return total_area; }
    const Color& get_color() const { return color; }
    void set_color(const Color& c) { color = c; }
};

// Класс Задача - объединяет поле и фигуры
class Puzzle {
private:
    std::shared_ptr<Grid> grid;
    std::shared_ptr<std::vector<Bundle>> bundles;
    std::string name;

public:
    Puzzle() = default;

    Puzzle(std::shared_ptr<Grid> g, std::shared_ptr<std::vector<Bundle>> b, std::string n = "Untitled")
        : grid(g), bundles(b), name(n) {}

    Puzzle(std::shared_ptr<Grid> g, const std::vector<Bundle>& b, std::string n = "Untitled")
        : grid(g), bundles(std::make_shared<std::vector<Bundle>>(b)), name(n) {}

    // глубокое копирование для солверов
    Puzzle clone() const;
    
    // Очистить сетку (стереть ответ)
    void clear_grid();

    std::shared_ptr<Grid> get_grid() const { return grid; }
    
    // Возвращаем ссылку на вектор внутри shared_ptr
    // const Puzzle дает const vector, но так как это shared_ptr, мы можем дать и не конст, если захотим.
    // Но лучше быть строгим: бандлы обычно immutable.
    const std::vector<Bundle>& get_bundles() const { return *bundles; }
    
    const std::string& get_name() const { return name; }
    void set_name(const std::string& n) { name = n; }
};
