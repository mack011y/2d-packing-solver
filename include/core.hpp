#pragma once
#include <vector>
#include <string>
#include <memory>
#include <map>
#include "graph.hpp"

// Типы сеток: квадратная, гексагональная, треугольная
enum class GridType {
    SQUARE,   // Квадратная (4 соседа)
    HEXAGON,  // Гексагональная (6 соседей)
    TRIANGLE  // Треугольная (3 соседа)
};

// Простая структура для цвета (RGB)
struct Color {
    int r, g, b;
};

// Данные для ячейки поля (координаты + информация о занятости)
struct GridCellData {
    int x, y;        // Координаты клетки на сетке
    int bundle_id;   // ID набора (бандла), который занял эту клетку (-1 если пусто)
    int figure_id;   // Уникальный ID фигуры на поле, -1 если пусто
    
    GridCellData(int x = 0, int y = 0) : x(x), y(y), bundle_id(-1), figure_id(-1) {}
};

// Поле для замощения (Grid)
// Представляет собой граф, где узлы - это клетки сетки.
// Наследуется от Graph<GridCellData>.
class Grid : public Graph<GridCellData> {
private:
    int width, height; // Размеры сетки
    GridType type;     // Тип топологии

public:
    // Конструктор сетки: инициализирует граф с нужным количеством портов
    // Гексагон: 6 портов, Треугольник: 3 порта, Квадрат: 4 порта
    Grid(int w, int h, GridType t) 
        : Graph<GridCellData>((t == GridType::HEXAGON ? 6 : (t == GridType::TRIANGLE ? 3 : 4))),
          width(w), height(h), type(t) {}

    int get_width() const { return width; }
    int get_height() const { return height; }
    GridType get_type() const { return type; }

    // Быстрый доступ к ID узла по координатам сетки (O(1))
    // Возвращает ID узла или -1, если координаты выходят за границы.
    int get_node_id_at(int x, int y) const {
        if (x < 0 || x >= width || y < 0 || y >= height) return -1;
        // Узлы генерируются построчно: id = y * width + x
        return y * width + x;
    }
};

// Данные для узла фигуры (пока пустая структура, но можно расширить)
struct FigureNodeData {};

// Фигура (Figure)
// Представляет собой связный подграф сетки, который нужно разместить.
// Наследуется от Graph<FigureNodeData>.
class Figure : public Graph<FigureNodeData> {
public:
    std::string name; // Имя фигуры (для отладки/визуализации)
    
    Figure(std::string n, int mp) : Graph<FigureNodeData>(mp), name(n) {}
};

// Набор фигур (Bundle)
// Группа фигур, которые должны быть размещены вместе (имеют общий ID и цвет).
class Bundle {
private:
    int id;                                      // Уникальный ID бандла
    std::vector<std::shared_ptr<Figure>> shapes; // Список фигур, входящих в этот набор
    size_t total_area;                           // Общая площадь (количество клеток)
    Color color;                                 // Цвет для визуализации

public:
    Bundle() : id(-1), total_area(0), color{255, 255, 255} {}

    // Конструктор с перемещением вектора фигур (для эффективности)
    Bundle(int id, std::vector<std::shared_ptr<Figure>> shapes, const Color& color)
        : id(id), shapes(std::move(shapes)), color(color) {
        recalculate_area();
    }

    // Пересчитывает общую площадь бандла
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
