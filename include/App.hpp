#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <memory>
#include <map>
#include <string>

#include "core.hpp"
#include "generators.h"
#include "solvers.h"
#include "ui/InputBox.hpp"
#include "utils/Timer.hpp"

class App {
public:
    App();
    void run();

private:
    // Window & Resources
    sf::RenderWindow window;
    sf::Font font;
    bool fontLoaded = false;
    sf::Texture hatchTexture;

    // Application State
    enum State { INIT, GENERATED, SOLVED };
    State currentState = INIT;
    std::string statusText = "Ready";
    std::string lastSolverName = "";
    double lastDuration = 0.0;
    float lastScore = 0.0;

    // Settings
    int gridW = 10;
    int gridH = 10;
    int minShapeSize = 3;
    int maxShapeSize = 5;
    int minBundleArea = 15;
    int maxBundleArea = 25;
    GridType selectedGridType = GridType::SQUARE;
    
    GeneratorConfig config;

    // Data
    std::shared_ptr<Grid> graph;
    std::vector<Bundle> bundles;
    std::map<int, int> solution_map;

    // UI & Layout
    float sidebarWidth = 320.0f;
    sf::FloatRect gridRect;
    sf::FloatRect sidebarRect;
    std::vector<std::shared_ptr<InputBox>> inputs;

    // Rendering Parameters
    float cellSize = 30.0f;
    float offsetX = 0.0f;
    float offsetY = 0.0f;

    // Colors
    sf::Color colorBg = sf::Color(20, 20, 20);
    sf::Color colorGridBg = sf::Color(30, 30, 30);
    sf::Color colorSidebarBg = sf::Color(35, 35, 40);
    sf::Color colorTextMain = sf::Color(220, 220, 220);
    sf::Color colorTextDim = sf::Color(150, 150, 150);
    sf::Color colorAccent = sf::Color(70, 130, 180);
    
    std::map<int, sf::Color> bundleColors;
    int hoveredBundleId = -1;

    // Methods
    void initUI();
    void createHatchTexture();
    void handleEvents();
    void render();
    void updateHover(int mx, int my);
    
    void applyConfig();
    void recalcLayout();
    void generatePuzzle();
    void solveDLX();
    void solveGRASP();

    void drawGridArea();
    void drawSidebar();
    void drawPanel(float x, float y, float w, float h);
    void drawText(const std::string& str, float x, float y, unsigned int size = 16, bool centered = false, sf::Color color = sf::Color(220, 220, 220));
    void drawThickLine(sf::Vector2f p1, sf::Vector2f p2, float thickness, sf::Color color);
    sf::Vector2f getNodePosition(int x, int y, GridType type);
};
