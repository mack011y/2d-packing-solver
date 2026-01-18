#pragma once
#include <SFML/Graphics.hpp>
#include <memory>
#include <vector>
#include <map>
#include <string>
#include "core.hpp"

// Класс Визуализатора
class Viewer {
public:
    Viewer();
    
    int run(const std::string& filename);

private:
    sf::RenderWindow window;
    sf::View view;          

    // Данные задачи
    std::shared_ptr<Grid> grid;
    std::vector<Bundle> bundles;
    std::map<int, sf::Color> bundleColors; // Кэш цветов для быстрого доступа
    
    // Состояние интерфейса
    float cellSize = 30.0f; // Размер одной клетки в пикселях
    bool isDragging = false;
    sf::Vector2i lastMousePos;
    int hoveredNodeId = -1; // ID узла под курсором мыши
 
    // Внутренние методы
    bool loadData(const std::string& filename); // Загрузка JSON
    bool handleEvents(); // Обработка мыши и клавиатуры
    bool render();       // Главный метод отрисовки кадра
    bool drawGrid();     // Рисование сетки и фигур
    
    // Конвертация координат сетки (X, Y) в координаты экрана (PixelX, PixelY)
    sf::Vector2f getNodePosition(int x, int y, GridType type);
    sf::Color getBundleColor(int id);
};
