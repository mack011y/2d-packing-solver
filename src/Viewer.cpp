#include "Viewer.hpp"
#include "utils/Serializer.hpp"
#include <cmath>
#include <iostream>
#include <cstdlib>
#include <algorithm>
#include <iomanip>
#include <filesystem>

Viewer::Viewer() {
    sf::ContextSettings settings;
    settings.antiAliasingLevel = 8;
    
    // Fullscreen mode
    sf::VideoMode desktop = sf::VideoMode::getDesktopMode();
    window.create(desktop, "Puzzle Viewer", sf::Style::None, sf::State::Fullscreen, settings);
    
    window.setFramerateLimit(60);
    
    createHatchTextures();
    
    view = window.getDefaultView();
    
    // Попытка загрузить шрифт (опционально)
    // 1. Пробуем системные шрифты (macOS/Linux)
    const std::vector<std::string> fontPaths = {
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/System/Library/Fonts/Arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "arial.ttf" // Рядом с бинарником
    };

    fontLoaded = false;
    for (const auto& path : fontPaths) {
        if (std::filesystem::exists(path) && font.openFromFile(path)) {
            fontLoaded = true;
            std::cout << "Loaded font: " << path << std::endl;
            break;
        }
    }
    
    if (!fontLoaded) {
        std::cerr << "Warning: No font found. Text will not be displayed." << std::endl;
    }
}

void Viewer::createHatchTextures() {
    // Создаем простую текстуру для штриховки (диагональные полосы)
    // Размер 16x16, повторяется
    sf::Image img(sf::Vector2u(16, 16), sf::Color::Transparent);
    sf::Color hatchColor(0, 0, 0, 200); // Черный контрастный
    
    for(unsigned int y=0; y<16; ++y) {
        for(unsigned int x=0; x<16; ++x) {
            // Диагональ: x + y
            if ((x + y) % 8 == 0 || (x + y) % 8 == 1) { 
                img.setPixel(sf::Vector2u(x, y), hatchColor);
            }
        }
    }
    hatchTexture.loadFromImage(img);
    hatchTexture.setRepeated(true);
    hatchTexture.setSmooth(true);
}

void Viewer::run(const std::string& filename) {
    if (!loadData(filename)) {
        // Не закрываем сразу, даем прочитать ошибку в консоли
        std::cerr << "Press Enter to exit..." << std::endl;
        std::cin.get();
        return;
    }
    
    while (window.isOpen()) {
        handleEvents();
        render();
    }
}

bool Viewer::loadData(const std::string& filename) {
    std::cout << "Loading " << filename << "..." << std::endl;
    try {
        auto result = Serializer::load_json(filename);
        std::cout << "JSON parse complete." << std::endl;
        grid = result.first;
        bundles = result.second;
        
        if (!grid) {
            std::cerr << "Error loading grid!" << std::endl;
            return false;
        }
        std::cout << "Grid loaded: " << grid->get_width() << "x" << grid->get_height() << std::endl;
        
        for(const auto& b : bundles) {
            bundleColors[b.get_id()] = sf::Color(b.get_color().r, b.get_color().g, b.get_color().b);
        }
        
        // Auto-Fit Logic
        float minX = 1e9, minY = 1e9, maxX = -1e9, maxY = -1e9;
        
        // Просто берем 4 угла сетки, чтобы не перебирать все точки (оптимизация)
        std::vector<sf::Vector2i> corners = {
            {0, 0}, 
            {grid->get_width()-1, 0}, 
            {0, grid->get_height()-1}, 
            {grid->get_width()-1, grid->get_height()-1}
        };

        for (const auto& c : corners) {
            sf::Vector2f pos = getNodePosition(c.x, c.y, grid->get_type());
            if (pos.x < minX) minX = pos.x;
            if (pos.x > maxX) maxX = pos.x;
            if (pos.y < minY) minY = pos.y;
            if (pos.y > maxY) maxY = pos.y;
        }
        
        float padding = 50.0f; 
        float gridW = maxX - minX + cellSize * 2.0f;
        float gridH = maxY - minY + cellSize * 2.0f;
        float centerX = (minX + maxX) / 2.0f;
        float centerY = (minY + maxY) / 2.0f;
        
        view.setCenter({centerX, centerY});
        
        sf::Vector2u winSize = window.getSize();
        float winRatio = (float)winSize.x / (float)winSize.y;
        float gridRatio = gridW / gridH;
        
        float zoomFactor = 1.0f;
        if (gridRatio > winRatio) {
            float visibleW = gridW + padding * 2.0f;
            zoomFactor = visibleW / (float)winSize.x;
        } else {
            float visibleH = gridH + padding * 2.0f;
            zoomFactor = visibleH / (float)winSize.y;
        }
        zoomFactor *= 1.2f; 
        
        view.setSize({(float)winSize.x * zoomFactor, (float)winSize.y * zoomFactor});
        std::cout << "Auto-fit complete." << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Exception loading data: " << e.what() << std::endl;
        return false;
    }
}

void Viewer::handleEvents() {
    while (const std::optional event = window.pollEvent()) {
        if (event->is<sf::Event::Closed>()) {
            window.close();
        }
        else if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
            if (keyPressed->scancode == sf::Keyboard::Scancode::Escape || 
                keyPressed->scancode == sf::Keyboard::Scancode::Q)
                window.close();
        }
        else if (const auto* mouseMoved = event->getIf<sf::Event::MouseMoved>()) {
            sf::Vector2f worldPos = window.mapPixelToCoords(mouseMoved->position, view);
            hoveredNodeId = -1;
            
            if (grid) {
                // ОПТИМИЗАЦИЯ: O(1) поиск вместо O(N)
                // Обратное преобразование координат (World -> Grid Index)
                int gx = -1, gy = -1;
                
                if (grid->get_type() == GridType::SQUARE) {
                     gx = std::round(worldPos.x / cellSize);
                     gy = std::round(worldPos.y / cellSize);
                } 
                else if (grid->get_type() == GridType::HEXAGON) {
                    // Аппроксимация для гексов (можно точнее, но для ховера сойдет)
                    // Расстояния между центрами
                    float size = cellSize;
                    float w = std::sqrt(3.0f) * size;
                    float x_spacing = w;
                    float y_spacing = 1.5f * size;
                    
                    gy = std::round(worldPos.y / y_spacing);
                    float offset = (gy % 2 != 0) ? (x_spacing / 2.0f) : 0.0f;
                    gx = std::round((worldPos.x - offset) / x_spacing);
                }
                else if (grid->get_type() == GridType::TRIANGLE) {
                    // Аппроксимация для треугольников
                    float size = cellSize;
                    float h = size * std::sqrt(3.0f) / 2.0f;
                    gx = std::round(worldPos.x / (size / 2.0f)); 
                    gy = std::round(worldPos.y / h);
                    // gx тут может быть примерно в 2 раза больше реального x, так как шаг x маленький
                    // Для простоты оставим старый перебор только для странных сеток или сделаем точнее
                    // Вернемся к быстрому перебору в локальной области?
                    // Нет, лучше просто проверить эту точку
                    gx = std::floor(worldPos.x / (size / 2.0f)); // Приближенно
                }

                // Проверяем кандидата и соседей (на случай ошибок округления)
                float minDist = cellSize * 0.8f;
                
                // Проверяем окно 3x3 вокруг предполагаемой точки для точности
                for(int dy = -1; dy <= 1; ++dy) {
                    for(int dx = -1; dx <= 1; ++dx) {
                         int nx = gx + dx;
                         int ny = gy + dy;
                         
                         int nid = grid->get_node_id_at(nx, ny);
                         if (nid != -1) {
                             sf::Vector2f pos = getNodePosition(nx, ny, grid->get_type());
                             // Центрирование
                             float cx = pos.x, cy = pos.y;
                             if (grid->get_type() == GridType::SQUARE) {
                                  cx += cellSize/2; cy += cellSize/2;
                             } else if (grid->get_type() == GridType::TRIANGLE) {
                                  bool is_up = ((nx + ny) % 2 == 0);
                                  float size = cellSize;
                                  float h = size * std::sqrt(3.0f) / 2.0f;
                                  cx += size/2.0f; 
                                  cy += (is_up ? h * 2.0f/3.0f : h/3.0f);
                             }
                             
                             float dist = std::sqrt(std::pow(cx - worldPos.x, 2) + std::pow(cy - worldPos.y, 2));
                             if (dist < minDist) {
                                 minDist = dist;
                                 hoveredNodeId = nid;
                             }
                         }
                    }
                }
            }
        }
    }
}

void Viewer::render() {
    window.clear(sf::Color(30, 30, 35)); // Приятный темно-серый фон
    window.setView(view);
    
    if (grid) {
        drawGrid();
    }
    
    window.setView(window.getDefaultView());
    drawOverlay();
    drawLegend();
    
    window.display();
}

sf::Vector2f Viewer::getNodePosition(int x, int y, GridType type) {
    float offsetX = 0; float offsetY = 0;
    if (type == GridType::SQUARE) {
        return {offsetX + x * cellSize, offsetY + y * cellSize};
    } 
    else if (type == GridType::HEXAGON) {
        float size = cellSize;
        float w = std::sqrt(3.0f) * size;
        float x_spacing = w;
        float y_spacing = 1.5f * size;
        float px = offsetX + x * x_spacing;
        float py = offsetY + y * y_spacing;
        if (y % 2 != 0) px += x_spacing / 2.0f;
        return {px, py};
    }
    else if (type == GridType::TRIANGLE) {
         float size = cellSize;
         float h = size * std::sqrt(3.0f) / 2.0f;
         float px = offsetX + x * (size / 2.0f);
         float py = offsetY + y * h;
         return {px, py};
    }
    return {0,0};
}

void Viewer::drawGrid() {
    int hoveredBundleId = -1;
    if (hoveredNodeId != -1 && grid) {
        hoveredBundleId = grid->get_node(hoveredNodeId).get_data().bundle_id;
    }

    // Цвет сетки (границ ячеек)
    sf::Color gridLineColor(20, 20, 20); 
    float outlineThick = -1.0f; // Внутренняя обводка, чтобы не вылезать за размеры

    for (auto const& node : grid->get_nodes()) {
        int bid = node.get_data().bundle_id;
        
        sf::Color cellColor = sf::Color(60, 60, 60);
        if (bid != -1 && bundleColors.count(bid)) {
            cellColor = bundleColors[bid];
        }

        sf::Vector2f pos = getNodePosition(node.get_data().x, node.get_data().y, grid->get_type());

        // Рисуем основную форму
        if (grid->get_type() == GridType::SQUARE) {
            sf::RectangleShape rect(sf::Vector2f(cellSize, cellSize));
            rect.setPosition(pos);
            rect.setFillColor(cellColor);
            rect.setOutlineColor(gridLineColor);
            rect.setOutlineThickness(outlineThick);
            
            window.draw(rect);

            // Если это активный бандл - рисуем штриховку поверх
            if (bid == hoveredBundleId && bid != -1) {
                rect.setFillColor(sf::Color::White); // Цвет штрихов (texture color modulation)
                rect.setTexture(&hatchTexture); // Накладываем текстуру
                // Вычисляем глобальные координаты текстуры для красивого эффекта
                sf::IntRect texRect({(int)pos.x, (int)pos.y}, {(int)cellSize, (int)cellSize}); 
                rect.setTextureRect(texRect);
                window.draw(rect);
            }
        }
        else if (grid->get_type() == GridType::HEXAGON) {
            float drawRadius = cellSize; 
            sf::ConvexShape hex(6);
            for(int i=0; i<6; ++i) {
                float angle_rad = (30.0f + i * 60.0f) * 3.14159265f / 180.0f;
                hex.setPoint(i, {drawRadius * std::cos(angle_rad), drawRadius * std::sin(angle_rad)});
            }
            hex.setPosition(pos); 
            hex.setFillColor(cellColor);
            hex.setOutlineColor(gridLineColor);
            hex.setOutlineThickness(outlineThick);

            window.draw(hex);
            
            if (bid == hoveredBundleId && bid != -1) {
                hex.setFillColor(sf::Color::White);
                hex.setTexture(&hatchTexture);
                sf::FloatRect b = hex.getGlobalBounds();
                hex.setTextureRect(sf::IntRect({(int)b.position.x, (int)b.position.y}, {(int)b.size.x, (int)b.size.y}));
                window.draw(hex);
            }
        }
        else if (grid->get_type() == GridType::TRIANGLE) {
            sf::ConvexShape tri(3);
            bool is_up = ((node.get_data().x + node.get_data().y) % 2 == 0);
            float size = cellSize;
            float h = size * std::sqrt(3.0f) / 2.0f;
            
            if (is_up) {
                tri.setPoint(0, {size/2, 0});     
                tri.setPoint(1, {size, h});   
                tri.setPoint(2, {0, h});          
            } else {
                tri.setPoint(0, {0, 0});              
                tri.setPoint(1, {size, 0});       
                tri.setPoint(2, {size/2, h}); 
            }
            tri.setPosition(pos);
            
            tri.setFillColor(cellColor);
            tri.setOutlineColor(gridLineColor);
            tri.setOutlineThickness(outlineThick);

            window.draw(tri);
            
            if (bid == hoveredBundleId && bid != -1) {
                tri.setFillColor(sf::Color::White);
                tri.setTexture(&hatchTexture);
                sf::FloatRect b = tri.getGlobalBounds();
                tri.setTextureRect(sf::IntRect({(int)b.position.x, (int)b.position.y}, {(int)b.size.x, (int)b.size.y}));
                window.draw(tri);
            }
        }
    }
}

void Viewer::drawOverlay() {
    if (!fontLoaded) return;
    
    sf::Text t(font);
    t.setCharacterSize(14);
    t.setFillColor(sf::Color::White);
    t.setPosition({10, 10});
    
    std::string info = "Viewer: Optimized Mode";
    
    if (hoveredNodeId != -1 && grid) {
        auto& data = grid->get_node(hoveredNodeId).get_data();
        info += "\nBundle ID: " + std::to_string(data.bundle_id);
        info += "\nCoords: (" + std::to_string(data.x) + ", " + std::to_string(data.y) + ")";
    }
    
    t.setString(info);
    
    sf::FloatRect bounds = t.getGlobalBounds();
    sf::RectangleShape bg(sf::Vector2f(bounds.size.x + 20, bounds.size.y + 20));
    bg.setPosition({5, 5});
    bg.setFillColor(sf::Color(0, 0, 0, 150));
    window.draw(bg);
    window.draw(t);
}

void Viewer::drawLegend() {
    if (!fontLoaded) return;
    
    // Параметры легенды
    float width = 300.0f;
    float height = 20.0f;
    sf::Vector2u winSize = window.getSize();
    float x = (winSize.x - width) / 2.0f;
    float y = winSize.y - 40.0f;
    
    // Градиентная полоса (из нескольких сегментов для радуги)
    int segments = 30; // Количество сегментов для плавности
    sf::VertexArray gradient(sf::PrimitiveType::TriangleStrip, (segments + 1) * 2);
    
    for(int i=0; i <= segments; ++i) {
        float t = (float)i / (float)segments;
        float px = x + t * width;
        
        // H: 240 (Blue) -> 0 (Red)
        float h = (1.0f - t) * (240.0f / 360.0f);
        
        // Manual HSV to RGB here (copy of generator logic)
        int r, g, b;
        float s = 0.85f, v = 0.95f; 
        int ii = int(h * 6);
        float f = h * 6 - ii;
        float p = v * (1 - s);
        float q = v * (1 - f * s);
        float tt = v * (1 - (1 - f) * s);
        float rf, gf, bf;
        switch (ii % 6) {
            case 0: rf = v; gf = tt; bf = p; break;
            case 1: rf = q; gf = v; bf = p; break;
            case 2: rf = p; gf = v; bf = tt; break;
            case 3: rf = p; gf = q; bf = v; break;
            case 4: rf = tt; gf = p; bf = v; break;
            case 5: rf = v; gf = p; bf = q; break;
            case 6: rf = v; gf = p; bf = q; break; // edge case
            default: rf=0; gf=0; bf=0; break;
        }
        r = int(rf * 255); g = int(gf * 255); b = int(bf * 255);
        
        gradient[i*2].position = {px, y};
        gradient[i*2].color = sf::Color(r, g, b);
        
        gradient[i*2+1].position = {px, y + height};
        gradient[i*2+1].color = sf::Color(r, g, b);
    }
    
    window.draw(gradient);
    
    // Подписи
    sf::Text textMin(font), textMax(font);
    textMin.setCharacterSize(14); textMax.setCharacterSize(14);
    textMin.setFillColor(sf::Color::White); textMax.setFillColor(sf::Color::White);
    
    textMin.setString("Small Area");
    textMax.setString("Big Area");
    
    sf::FloatRect bMin = textMin.getLocalBounds();
    // sf::FloatRect bMax = textMax.getLocalBounds();
    
    textMin.setPosition({x - bMin.size.x - 10, y});
    textMax.setPosition({x + width + 10, y});
    
    // Фоновые плашки под текст для читаемости
    sf::RectangleShape bgMin(sf::Vector2f(bMin.size.x + 4, bMin.size.y + 4));
    bgMin.setFillColor(sf::Color(0,0,0,150));
    bgMin.setPosition(textMin.getPosition());
    
    // window.draw(bgMin); // Опционально
    window.draw(textMin);
    window.draw(textMax);
}
