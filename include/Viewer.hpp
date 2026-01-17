#pragma once
#include <SFML/Graphics.hpp>
#include <memory>
#include <vector>
#include <map>
#include <string>
#include "core.hpp"

// Класс Визуализатора
// Отвечает за создание окна, обработку ввода и отрисовку состояния задачи.
class Viewer {
public:
    Viewer();
    // Основной цикл приложения
    void run(const std::string& filename);

private:
    sf::RenderWindow window;
    sf::View view;          // Камера (для зума и панорамирования)
    sf::Font font;
    bool fontLoaded = false;

    // Данные задачи
    std::shared_ptr<Grid> grid;
    std::vector<Bundle> bundles;
    std::map<int, sf::Color> bundleColors; // Кэш цветов для быстрого доступа
    
    // Состояние интерфейса
    float cellSize = 30.0f; // Размер одной клетки в пикселях
    bool isDragging = false;
    sf::Vector2i lastMousePos;
    int hoveredNodeId = -1; // ID узла под курсором мыши
    
    // Ресурсы
    sf::Texture hatchTexture; // Текстура штриховки для выделения

    // Внутренние методы
    void createHatchTextures(); // Генерация текстур программно
    bool loadData(const std::string& filename); // Загрузка JSON
    void handleEvents(); // Обработка мыши и клавиатуры
    void render();       // Главный метод отрисовки кадра
    void drawGrid();     // Рисование сетки и фигур
    void drawOverlay();  // Рисование текста поверх (HUD)
    void drawLegend();   // Рисование легенды (цветовой шкалы)
    
    // Конвертация координат сетки (X, Y) в координаты экрана (PixelX, PixelY)
    sf::Vector2f getNodePosition(int x, int y, GridType type);
    sf::Color getBundleColor(int id);
};
