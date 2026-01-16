#pragma once
#include <SFML/Graphics.hpp>
#include <memory>
#include <vector>
#include <map>
#include <string>
#include "core.hpp"

class Viewer {
public:
    Viewer();
    void run(const std::string& filename);

private:
    sf::RenderWindow window;
    sf::View view;
    sf::Font font;
    bool fontLoaded = false;

    // Data
    std::shared_ptr<Grid> grid;
    std::vector<Bundle> bundles;
    std::map<int, sf::Color> bundleColors;
    
    // UI State
    float cellSize = 30.0f;
    bool isDragging = false;
    sf::Vector2i lastMousePos;
    int hoveredNodeId = -1;
    
    // Resources
    sf::Texture hatchTexture;

    // Methods
    void createHatchTextures();
    bool loadData(const std::string& filename);
    void handleEvents();
    void render();
    void drawGrid();
    void drawOverlay();
    void drawLegend();
    
    sf::Vector2f getNodePosition(int x, int y, GridType type);
    sf::Color getBundleColor(int id);
};
