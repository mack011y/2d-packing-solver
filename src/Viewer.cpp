#include "Viewer.hpp"
#include "utils/Serializer.hpp"
#include "utils/ColorUtils.hpp"
#include <cmath>
#include <iostream>
#include <cstdlib>
#include <algorithm>
#include <iomanip>
#include <filesystem>
#include <optional>

Viewer::Viewer() {
    sf::ContextSettings settings;
    settings.antiAliasingLevel = 8;
    
    window.create(sf::VideoMode({1280, 800}), "CLEAN VIEWER", sf::Style::Default, sf::State::Windowed, settings);
    
    window.setFramerateLimit(60);
    
    view = window.getDefaultView();
}

int Viewer::run(const std::string& filename) {
    if (!loadData(filename)) {
        std::cerr << "Failed to load data: " << filename << std::endl;
        return 1;
    }
    
    while (window.isOpen()) {
        if (!handleEvents()) break;
        if (!render()) {
            std::cerr << "Render error" << std::endl;
            break;
        }
    }
    return 0;
}

bool Viewer::loadData(const std::string& filename) {
    std::cout << "Loading " << filename << "..." << std::endl;
    try {
        if (!std::filesystem::exists(filename)) {
            std::cerr << "File not found: " << filename << std::endl;
            return false;
        }

        auto puzzle = Serializer::load(filename);
        std::cout << "JSON parse complete." << std::endl;
        grid = puzzle.get_grid();
        bundles = puzzle.get_bundles();
        
        if (!grid) {
            std::cerr << "Error loading grid!" << std::endl;
            return false;
        }
        std::cout << "Grid loaded: " << grid->get_width() << "x" << grid->get_height() << std::endl;
        
        for(const auto& b : bundles) {
            bundleColors[b.get_id()] = sf::Color(b.get_color().r, b.get_color().g, b.get_color().b);
        }
        
        float minX = 1e9, minY = 1e9, maxX = -1e9, maxY = -1e9;
        
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
        std::cout << "Auto-fit complete. Zoom: " << zoomFactor 
                  << " Bounds: [" << minX << "," << minY << "] -> [" << maxX << "," << maxY << "]" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Exception loading data: " << e.what() << std::endl;
        return false;
    }
}

bool Viewer::handleEvents() {
    while (const std::optional event = window.pollEvent()) {
        if (event->is<sf::Event::Closed>()) {
            window.close();
            return false;
        }
        else if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
            if (keyPressed->code == sf::Keyboard::Key::Escape || 
                keyPressed->code == sf::Keyboard::Key::Q) {
                window.close();
                return false;
            }
        }
        else if (const auto* mouseMoved = event->getIf<sf::Event::MouseMoved>()) {
            sf::Vector2f worldPos = window.mapPixelToCoords(mouseMoved->position, view);
            hoveredNodeId = -1;
            
            if (grid) {
                int gx = -1, gy = -1;
                
                if (grid->get_type() == GridType::SQUARE) {
                     gx = std::round(worldPos.x / cellSize);
                     gy = std::round(worldPos.y / cellSize);
                } 
                else if (grid->get_type() == GridType::HEXAGON) {
                    float size = cellSize;
                    float x_spacing = std::sqrt(3.0f) * size;
                    float y_spacing = 1.5f * size;
                    
                    gy = std::round(worldPos.y / y_spacing);
                    float offset = (gy % 2 != 0) ? (x_spacing / 2.0f) : 0.0f;
                    gx = std::round((worldPos.x - offset) / x_spacing);
                }
                else if (grid->get_type() == GridType::TRIANGLE) {
                    float size = cellSize;
                    float h = size * std::sqrt(3.0f) / 2.0f;
                    gx = std::floor(worldPos.x / (size / 2.0f)); 
                    gy = std::round(worldPos.y / h);
                }

                float minDist = cellSize * 0.8f;
                
                for(int dy = -1; dy <= 1; ++dy) {
                    for(int dx = -1; dx <= 1; ++dx) {
                         int nx = gx + dx;
                         int ny = gy + dy;
                         
                         int nid = grid->get_node_id_at(nx, ny);
                         if (nid != -1) {
                             sf::Vector2f pos = getNodePosition(nx, ny, grid->get_type());
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
    return true;
}

bool Viewer::render() {
    window.clear(sf::Color(30, 30, 35));
    
    window.setView(view);
    if (grid) {
        if (!drawGrid()) return false;
    }
    
    window.display();
    return true;
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

bool Viewer::drawGrid() {
    int hoveredBundleId = -1;
    if (hoveredNodeId != -1 && grid) {
        hoveredBundleId = grid->get_node(hoveredNodeId).get_data().bundle_id;
    }

    sf::Color gridLineColor(100, 100, 100); 
    float outlineThick = 1.0f; 

    for (auto const& node : grid->get_nodes()) {
        int bid = node.get_data().bundle_id;
        
        sf::Color cellColor = sf::Color(60, 60, 60);
        if (bid != -1 && bundleColors.count(bid)) {
            cellColor = bundleColors[bid];
        }

        sf::Vector2f pos = getNodePosition(node.get_data().x, node.get_data().y, grid->get_type());

        if (grid->get_type() == GridType::SQUARE) {
            sf::RectangleShape rect(sf::Vector2f(cellSize, cellSize));
            rect.setPosition(pos);
            rect.setFillColor(cellColor);
            rect.setOutlineColor(gridLineColor);
            rect.setOutlineThickness(outlineThick);
            
            window.draw(rect);

            if (bid == hoveredBundleId && bid != -1) {
                rect.setFillColor(sf::Color(255, 255, 255, 100)); 
                rect.setOutlineColor(sf::Color::White);
                rect.setOutlineThickness(2.0f);
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
                hex.setFillColor(sf::Color(255, 255, 255, 100));
                hex.setOutlineColor(sf::Color::White);
                hex.setOutlineThickness(2.0f);
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
                tri.setFillColor(sf::Color(255, 255, 255, 100));
                tri.setOutlineColor(sf::Color::White);
                tri.setOutlineThickness(2.0f);
                window.draw(tri);
            }
        }
    }
    return true;
}
