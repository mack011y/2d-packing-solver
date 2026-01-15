#pragma once
#include <vector>
#include <string>
#include <memory>
#include <map>
#include "graph.hpp"

enum class GridType {
    SQUARE,   // 4
    HEXAGON,  // 6
    TRIANGLE  // 3
};

struct Color {
    int r, g, b;
};

// Данные для ячейки поля (координаты + кто занял)
struct GridCellData {
    int x, y;
    int bundle_id;   // ID бандла, -1 если пусто
    int figure_id;   // Уникальный ID фигуры на поле, -1 если пусто
    
    GridCellData(int x = 0, int y = 0) : x(x), y(y), bundle_id(-1), figure_id(-1) {}
};

// Поле для замощения (Рюкзак)
class Grid : public Graph<GridCellData> {
private:
    int width, height;
    GridType type;

public:
    Grid(int w, int h, GridType t) 
        : Graph<GridCellData>((t == GridType::HEXAGON ? 6 : (t == GridType::TRIANGLE ? 3 : 4))),
          width(w), height(h), type(t) {}

    int get_width() const { return width; }
    int get_height() const { return height; }
    GridType get_type() const { return type; }
};

// Данные для узла фигуры (пока ниче, но в целом можно и чет хранить)
struct FigureNodeData {};

// Фигура (тоже граф)
class Figure : public Graph<FigureNodeData> {
public:
    std::string name;
    
    Figure(std::string n, int mp) : Graph<FigureNodeData>(mp), name(n) {}
};

// Класс набора фигур
class Bundle {
private:
    int id;
    std::vector<std::shared_ptr<Figure>> shapes; // Фигуры - это графы
    size_t total_area;
    Color color;

public:
    Bundle() : id(-1), total_area(0), color{255, 255, 255} {}

    Bundle(int id, const std::vector<std::shared_ptr<Figure>>& shapes, Color color)
        : id(id), shapes(shapes), color(color) {
        recalculate_area();
    }

    void recalculate_area() {
        total_area = 0;
        for(const auto& s : shapes) {
            total_area += s->size();
        }
    }

    int get_id() const { return id; }
    const std::vector<std::shared_ptr<Figure>>& get_shapes() const { return shapes; }
    size_t get_total_area() const { return total_area; }
    const Color& get_color() const { return color; }
    void set_color(const Color& c) { color = c; }
};
