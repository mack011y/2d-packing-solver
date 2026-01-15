#include "App.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cmath>

App::App() {
    window.create(sf::VideoMode({1200, 800}), "Graph Tiling C++ Interface");
    window.setFramerateLimit(60);

    std::vector<std::string> fontPaths = {
        "/System/Library/Fonts/Helvetica.ttc",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/Library/Fonts/Arial.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
    };
    
    for (const auto& path : fontPaths) {
        if (font.openFromFile(path)) {
            fontLoaded = true;
            break;
        }
    }
    
    if (!fontLoaded) {
        std::cerr << "Warning: No font found. Text will not display." << std::endl;
    }

    config = {gridW, gridH, minShapeSize, maxShapeSize, minBundleArea, maxBundleArea, selectedGridType}; 
    recalcLayout();
    initUI();
    createHatchTexture();
}

void App::run() {
    while (window.isOpen()) {
        handleEvents();
        render();
    }
}

void App::createHatchTexture() {
    sf::Image img;
    img.resize({32, 32}, sf::Color::Transparent);
    for(unsigned int y=0; y<32; ++y) {
        for(unsigned int x=0; x<32; ++x) {
            if ((x + y) % 8 == 0 || (x + y) % 8 == 1) { 
                img.setPixel({x, y}, sf::Color(255, 255, 255, 180));
            }
        }
    }
    if (!hatchTexture.loadFromImage(img)) {
        std::cerr << "Failed to load hatch texture" << std::endl;
    }
    hatchTexture.setRepeated(true);
}

void App::initUI() {
    inputs.clear();
    float sx = window.getSize().x - sidebarWidth + 20;
    float sy = 260; 
    
    inputs.push_back(std::make_shared<InputBox>(sx, sy, 60, 25, "Grid W", &gridW, font));
    inputs.push_back(std::make_shared<InputBox>(sx + 80, sy, 60, 25, "Grid H", &gridH, font));
    
    inputs.push_back(std::make_shared<InputBox>(sx, sy + 50, 60, 25, "Min Shape", &minShapeSize, font));
    inputs.push_back(std::make_shared<InputBox>(sx + 80, sy + 50, 60, 25, "Max Shape", &maxShapeSize, font));

    inputs.push_back(std::make_shared<InputBox>(sx, sy + 100, 60, 25, "Min Area", &minBundleArea, font));
    inputs.push_back(std::make_shared<InputBox>(sx + 80, sy + 100, 60, 25, "Max Area", &maxBundleArea, font));
}

void App::handleEvents() {
    while (const auto event = window.pollEvent()) {
        if (event->is<sf::Event::Closed>()) {
            window.close();
        }
        
        for(auto& inp : inputs) inp->handleEvent(*event);
        
        if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>()) {
            if (mousePressed->button == sf::Mouse::Button::Left) {
                float mx = (float)mousePressed->position.x;
                float my = (float)mousePressed->position.y;
                
                bool clickedAny = false;
                for(auto& inp : inputs) {
                    if (inp->contains(mx, my)) {
                        inp->isFocused = true;
                        clickedAny = true;
                    } else {
                        inp->isFocused = false;
                        inp->update(); 
                    }
                }
            }
        }
        
        if (const auto* resized = event->getIf<sf::Event::Resized>()) {
            sf::FloatRect visibleArea({0, 0}, {(float)resized->size.x, (float)resized->size.y});
            window.setView(sf::View(visibleArea));
            recalcLayout();
            initUI(); 
        }

        if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
            bool inputFocused = false;
            for(auto& inp : inputs) if(inp->isFocused) inputFocused = true;
            
            if (inputFocused) {
                if (keyPressed->scancode == sf::Keyboard::Scancode::Enter) {
                    for(auto& inp : inputs) { inp->isFocused = false; inp->update(); }
                    applyConfig();
                    generatePuzzle();
                }
                 continue; 
            }

            if (keyPressed->scancode == sf::Keyboard::Scancode::Escape) window.close();

            if (keyPressed->scancode == sf::Keyboard::Scancode::G) {
                applyConfig();
                generatePuzzle();
            }

            if (keyPressed->scancode == sf::Keyboard::Scancode::D) solveDLX();
            if (keyPressed->scancode == sf::Keyboard::Scancode::S) solveGRASP();
            
            // Toggle Grid Type
            if (keyPressed->scancode == sf::Keyboard::Scancode::T) {
                if (selectedGridType == GridType::SQUARE) selectedGridType = GridType::HEXAGON;
                else if (selectedGridType == GridType::HEXAGON) selectedGridType = GridType::TRIANGLE;
                else selectedGridType = GridType::SQUARE;
                
                applyConfig();
                generatePuzzle(); 
            }
        }
        
        if (const auto* mouseMoved = event->getIf<sf::Event::MouseMoved>()) {
            updateHover(mouseMoved->position.x, mouseMoved->position.y);
        }
    }
}

void App::applyConfig() {
    // Validation and Auto-correction
    if (gridW < 2) gridW = 2;
    if (gridH < 2) gridH = 2;
    
    if (minShapeSize < 1) minShapeSize = 1;
    if (maxShapeSize < 1) maxShapeSize = 1;
    if (minShapeSize > maxShapeSize) std::swap(minShapeSize, maxShapeSize);
    
    if (minBundleArea < 1) minBundleArea = 1;
    if (maxBundleArea < 1) maxBundleArea = 1;
    if (minBundleArea > maxBundleArea) std::swap(minBundleArea, maxBundleArea);
    
    // Logical constraint
    if (minBundleArea < minShapeSize) minBundleArea = minShapeSize;
    if (maxBundleArea < minBundleArea) maxBundleArea = minBundleArea;

    config.width = gridW;
    config.height = gridH;
    config.min_shape_size = minShapeSize;
    config.max_shape_size = maxShapeSize;
    config.min_bundle_area = minBundleArea;
    config.max_bundle_area = maxBundleArea;
    config.grid_type = selectedGridType;
    recalcLayout();
}

sf::Vector2f App::getNodePosition(int x, int y, GridType type) {
    if (type == GridType::SQUARE) {
        return {offsetX + x * cellSize, offsetY + y * cellSize};
    } 
    else if (type == GridType::HEXAGON) {
        float size = cellSize;
        float w = sqrt(3.0f) * size;
        
        float x_spacing = w;
        float y_spacing = 1.5f * size;
        
        float px = offsetX + x * x_spacing;
        float py = offsetY + y * y_spacing;
        
        if (y % 2 != 0) {
            px += x_spacing / 2.0f;
        }
        
        return {px, py};
    }
    else if (type == GridType::TRIANGLE) {
         float size = cellSize;
         float h = size * sqrt(3) / 2.0f;
         
         float px = offsetX + x * (size / 2.0f);
         float py = offsetY + y * h;
         
         return {px, py};
    }
    return {0,0};
}

void App::updateHover(int mx, int my) {
    hoveredBundleId = -1;
    if (!graph) return;
    
    float minDist = 999999.0f;
    int bestId = -1;
    
    float mxf = (float)mx;
    float myf = (float)my;

    if (!gridRect.contains({mxf, myf})) return;

    for(auto const& node : graph->get_nodes()) {
        int id = node.get_id();
        sf::Vector2f pos = getNodePosition(node.get_data().x, node.get_data().y, graph->get_type());
        float cx, cy;
        
        if (graph->get_type() == GridType::SQUARE) {
            cx = pos.x + cellSize/2; 
            cy = pos.y + cellSize/2;
        } else if (graph->get_type() == GridType::HEXAGON) {
             cx = pos.x; 
             cy = pos.y;
        } else { // Triangle
             bool is_up = ((node.get_data().x + node.get_data().y) % 2 == 0);
             float size = cellSize;
             float h = size * sqrt(3) / 2.0f;
             cx = pos.x + size/2.0f; 
             cy = pos.y + (is_up ? h * 2.0f/3.0f : h/3.0f); 
        }
        
        float dx = mxf - cx;
        float dy = myf - cy;
        float d2 = dx*dx + dy*dy;
        
        if (d2 < (cellSize)*(cellSize) * 4 && d2 < minDist) {
            minDist = d2;
            bestId = id;
        }
    }
    
    if (bestId != -1) {
         if (currentState == SOLVED) {
             try {
                hoveredBundleId = graph->get_node(bestId).get_data().bundle_id;
             } catch(...) {}
         } else {
             if(solution_map.count(bestId)) hoveredBundleId = solution_map[bestId];
         }
    }
}

void App::recalcLayout() {
    sf::Vector2u size = window.getSize();
    float w = static_cast<float>(size.x);
    float h = static_cast<float>(size.y);

    sidebarRect = sf::FloatRect({w - sidebarWidth, 0}, {sidebarWidth, h});
    gridRect = sf::FloatRect({0, 0}, {w - sidebarWidth, h});

    if (graph || (config.width > 0 && config.height > 0)) {
        float availW = gridRect.size.x - 60.0f; 
        float availH = gridRect.size.y - 60.0f;
        
        float totalW = 0, totalH = 0;
        
        if (selectedGridType == GridType::SQUARE) {
            cellSize = std::min(availW / config.width, availH / config.height);
            totalW = cellSize * config.width;
            totalH = cellSize * config.height;
        } else if (selectedGridType == GridType::HEXAGON) {
            float aspectW = 1.732f * (config.width + 0.5f);
            float aspectH = 1.5f * config.height + 0.5f;
            
            float s1 = availW / aspectW;
            float s2 = availH / aspectH;
            cellSize = std::min(s1, s2);
            
            totalW = aspectW * cellSize;
            totalH = aspectH * cellSize;
        } else { // TRIANGLE
             float aspectW = (config.width * 0.5f + 0.5f);
             float aspectH = (config.height * 0.866f);
             
             float s1 = availW / aspectW;
             float s2 = availH / aspectH;
             cellSize = std::min(s1, s2);
             
             totalW = aspectW * cellSize;
             totalH = aspectH * cellSize;
        }

        if (cellSize < 5.0f) cellSize = 5.0f;

        offsetX = gridRect.position.x + (gridRect.size.x - totalW) / 2.0f;
        offsetY = gridRect.position.y + (gridRect.size.y - totalH) / 2.0f;
        
        if (selectedGridType == GridType::HEXAGON) {
             offsetX += cellSize * 0.866f; 
             offsetY += cellSize;
        }
    }
}

void App::generatePuzzle() {
    Timer timer;
    timer.start();
    
    PuzzleGenerator generator(config);
    auto res = generator.generate(graph);
    
    bundles = res.first;
    solution_map = res.second;
    
    bundleColors.clear();
    for (const auto& b : bundles) {
         Color c = b.get_color();
         bundleColors[b.get_id()] = sf::Color(c.r, c.g, c.b);
    }

    lastDuration = timer.get_elapsed_sec();
    currentState = GENERATED;
    statusText = "Generated Puzzle";
    lastSolverName = "Generator";
    lastScore = 0.0f; 
    
    recalcLayout(); 
}

void App::solveDLX() {
    if (currentState == INIT) return;
    for (auto& node : graph->get_nodes()) {
         node.get_data().bundle_id = -1;
         node.get_data().figure_id = -1;
    }
    
    DLXSolver solver(graph, bundles);
    Timer timer; timer.start();
    lastScore = solver.solve();
    lastDuration = timer.get_elapsed_sec();
    currentState = SOLVED;
    lastSolverName = "DLX Exact";
    statusText = (lastScore > 0) ? "Solved" : "Failed";
}

void App::solveGRASP() {
    if (currentState == INIT) return;
    for (auto& node : graph->get_nodes()) {
        node.get_data().bundle_id = -1;
        node.get_data().figure_id = -1;
    }
    
    GRASPSolver solver(graph, bundles);
    solver.max_iterations = 200; 
    solver.alpha = 0.8f;
    Timer timer; timer.start();
    lastScore = solver.solve();
    lastDuration = timer.get_elapsed_sec();
    currentState = SOLVED;
    lastSolverName = "GRASP";
    statusText = (lastScore > 0) ? "Solved" : "Failed";
}

void App::render() {
    window.clear(colorBg);
    drawGridArea();
    drawSidebar();
    window.display();
}

void App::drawThickLine(sf::Vector2f p1, sf::Vector2f p2, float thickness, sf::Color color) {
    sf::Vector2f dir = p2 - p1;
    float len = std::sqrt(dir.x*dir.x + dir.y*dir.y);
    if(len < 0.1f) return;
    
    float angle = std::atan2(dir.y, dir.x) * 180.0f / 3.14159265f;
    
    sf::RectangleShape line(sf::Vector2f(len, thickness));
    line.setOrigin({0, thickness / 2.0f});
    line.setPosition(p1);
    line.setRotation(sf::degrees(angle));
    line.setFillColor(color);
    
    window.draw(line);
}

void App::drawGridArea() {
    sf::RectangleShape bg(gridRect.size);
    bg.setPosition(gridRect.position);
    bg.setFillColor(colorGridBg);
    window.draw(bg);
    
    if (currentState == INIT && !graph) {
        drawText("Press 'G' to Generate", gridRect.position.x + gridRect.size.x/2, gridRect.position.y + gridRect.size.y/2, 24, true);
        return;
    }

    // Pass 1: Draw Cells
    for (auto const& node : graph->get_nodes()) {
        int id = node.get_id();
        int bid = -1;
        
        if (currentState == SOLVED) bid = node.get_data().bundle_id;
        else if (solution_map.count(id)) bid = solution_map[id];

        sf::Color cellColor = sf::Color(60, 60, 60);
        if (bid != -1 && bundleColors.count(bid)) {
            cellColor = bundleColors[bid];
        }
        sf::Color hoverColor = sf::Color(
            255 - cellColor.r,
            255 - cellColor.g,
            255 - cellColor.b
        );

        sf::Vector2f pos = getNodePosition(node.get_data().x, node.get_data().y, graph->get_type());

        if (graph->get_type() == GridType::SQUARE) {
            sf::RectangleShape rect(sf::Vector2f(cellSize, cellSize));
            rect.setPosition(pos);
            rect.setFillColor(cellColor);
            rect.setSize({cellSize+0.5f, cellSize+0.5f}); 
            window.draw(rect);
            
            if (bid == hoveredBundleId && bid != -1) {
                sf::RectangleShape overlay = rect;
                overlay.setFillColor(hoverColor);
                overlay.setTexture(&hatchTexture);
                window.draw(overlay);
            }
        }
        else if (graph->get_type() == GridType::HEXAGON) {
            float drawRadius = cellSize; 
            sf::CircleShape hex(drawRadius, 6);
            hex.setRotation(sf::degrees(0)); 
            hex.setOrigin({drawRadius, drawRadius});
            hex.setPosition(pos); 
            hex.setFillColor(cellColor);
            window.draw(hex);
            
            if (bid == hoveredBundleId && bid != -1) {
                sf::CircleShape overlay = hex;
                overlay.setFillColor(hoverColor);
                overlay.setTexture(&hatchTexture);
                window.draw(overlay);
            }
        }
        else if (graph->get_type() == GridType::TRIANGLE) {
            sf::ConvexShape tri(3);
            bool is_up = ((node.get_data().x + node.get_data().y) % 2 == 0);
            float size = cellSize;
            float h = size * sqrt(3) / 2.0f;
            float px = pos.x;
            float py = pos.y;
            
            if (is_up) {
                tri.setPoint(0, {px + size/2, py});     
                tri.setPoint(1, {px + size, py + h});   
                tri.setPoint(2, {px, py + h});          
            } else {
                tri.setPoint(0, {px, py});              
                tri.setPoint(1, {px + size, py});       
                tri.setPoint(2, {px + size/2, py + h}); 
            }
            
            tri.setFillColor(cellColor);
            window.draw(tri);
            
            if (bid == hoveredBundleId && bid != -1) {
                sf::ConvexShape overlay = tri;
                overlay.setFillColor(hoverColor);
                overlay.setTexture(&hatchTexture);
                window.draw(overlay);
            }
        }
    }

    // Pass 2: Draw Borders (Smart Outlines)
    float thickness = 3.0f;
    sf::Color borderColor = sf::Color(20, 20, 20); 

    if (graph->get_type() == GridType::SQUARE) {
        for (auto const& node : graph->get_nodes()) {
            int my_fid = node.get_data().figure_id;
            sf::Vector2f pos = getNodePosition(node.get_data().x, node.get_data().y, GridType::SQUARE);
            
            struct Edge { int port; sf::Vector2f p1; sf::Vector2f p2; };
            std::vector<Edge> edges = {
                {0, pos, {pos.x + cellSize, pos.y}},                     
                {1, {pos.x + cellSize, pos.y}, {pos.x + cellSize, pos.y + cellSize}}, 
                {2, {pos.x + cellSize, pos.y + cellSize}, {pos.x, pos.y + cellSize}}, 
                {3, {pos.x, pos.y + cellSize}, pos}                      
            };
            
            for(const auto& edge : edges) {
                int nid = node.get_neighbor(edge.port);
                int n_fid = -2; 
                if (nid != -1) n_fid = graph->get_node(nid).get_data().figure_id;
                
                if (my_fid != n_fid) {
                    if (!(my_fid == -1 && n_fid == -1)) {
                         drawThickLine(edge.p1, edge.p2, thickness, borderColor);
                    }
                }
            }
        }
    }
    else if (graph->get_type() == GridType::HEXAGON) {
        for (auto const& node : graph->get_nodes()) {
            int my_fid = node.get_data().figure_id;
            sf::Vector2f pos = getNodePosition(node.get_data().x, node.get_data().y, GridType::HEXAGON);
            float cx = pos.x, cy = pos.y, R = cellSize;
            
            std::vector<sf::Vector2f> verts;
            for(int i=0; i<6; ++i) {
                float angle_rad = (30.0f + i * 60.0f) * 3.14159265f / 180.0f;
                verts.push_back({cx + R * std::cos(angle_rad), cy + R * std::sin(angle_rad)});
            }
            
            for(int i=0; i<6; ++i) {
                sf::Vector2f p1 = verts[i], p2 = verts[(i+1)%6];
                sf::Vector2f mid = (p1 + p2) * 0.5f;
                
                int best_nid = -1; float min_d2 = 999999.0f;
                for(int p=0; p<6; ++p) {
                    int nid = node.get_neighbor(p);
                    if(nid != -1) {
                        const auto& n_node = graph->get_node(nid);
                        sf::Vector2f n_pos = getNodePosition(n_node.get_data().x, n_node.get_data().y, GridType::HEXAGON);
                        float dx = n_pos.x - mid.x, dy = n_pos.y - mid.y;
                        float d2 = dx*dx + dy*dy;
                        if (d2 < min_d2) { min_d2 = d2; best_nid = nid; }
                    }
                }
                if (min_d2 > cellSize*cellSize) best_nid = -1;
                
                int n_fid = -2;
                if (best_nid != -1) n_fid = graph->get_node(best_nid).get_data().figure_id;
                
                if (my_fid != n_fid) {
                    if (!(my_fid == -1 && n_fid == -1)) drawThickLine(p1, p2, thickness, borderColor);
                }
            }
        }
    }
    else if (graph->get_type() == GridType::TRIANGLE) {
        for (auto const& node : graph->get_nodes()) {
            int my_fid = node.get_data().figure_id;
            sf::Vector2f pos = getNodePosition(node.get_data().x, node.get_data().y, GridType::TRIANGLE);
            
            bool is_up = ((node.get_data().x + node.get_data().y) % 2 == 0);
            float size = cellSize, h = size * sqrt(3) / 2.0f;
            float px = pos.x, py = pos.y;
            
            std::vector<sf::Vector2f> verts;
            if (is_up) {
                verts.push_back({px + size/2, py});     
                verts.push_back({px + size, py + h});   
                verts.push_back({px, py + h});          
            } else {
                verts.push_back({px, py});              
                verts.push_back({px + size, py});       
                verts.push_back({px + size/2, py + h}); 
            }
            
            for(int i=0; i<3; ++i) {
                sf::Vector2f p1 = verts[i], p2 = verts[(i+1)%3];
                sf::Vector2f mid = (p1 + p2) * 0.5f;
                int best_nid = -1; float min_d2 = 999999.0f;
                for(int p=0; p<3; ++p) {
                    int nid = node.get_neighbor(p);
                    if(nid != -1) {
                         const auto& n_node = graph->get_node(nid);
                         sf::Vector2f n_pos = getNodePosition(n_node.get_data().x, n_node.get_data().y, GridType::TRIANGLE);
                         float n_cx = n_pos.x + size/2.0f; 
                         float n_cy = n_pos.y + (((n_node.get_data().x+n_node.get_data().y)%2==0) ? h*2/3 : h/3);
                         float dx = n_cx - mid.x, dy = n_cy - mid.y;
                         float d2 = dx*dx + dy*dy;
                         if (d2 < min_d2) { min_d2 = d2; best_nid = nid; }
                    }
                }
                if (min_d2 > cellSize*cellSize) best_nid = -1;
                int n_fid = -2;
                if (best_nid != -1) n_fid = graph->get_node(best_nid).get_data().figure_id;
                
                if (my_fid != n_fid) {
                     if (!(my_fid == -1 && n_fid == -1)) drawThickLine(p1, p2, thickness, borderColor);
                }
            }
        }
    }
}

void App::drawSidebar() {
    sf::RectangleShape bg(sidebarRect.size);
    bg.setPosition(sidebarRect.position);
    bg.setFillColor(colorSidebarBg);
    window.draw(bg);
    
    float x = sidebarRect.position.x + 20.0f;
    float y = sidebarRect.position.y + 20.0f;

    drawText("Graph Tiling", x, y, 24, false, sf::Color::White);
    y += 40;

    drawPanel(x, y, sidebarWidth - 40, 140);
    float py = y + 10;
    drawText("STATUS", x + 10, py, 18, false, colorAccent); py += 25;
    
    std::stringstream ss;
    ss << "State: " << statusText;
    drawText(ss.str(), x + 10, py, 14); py += 20;

    std::string typeStr = "SQUARE";
    if (selectedGridType == GridType::HEXAGON) typeStr = "HEXAGON";
    if (selectedGridType == GridType::TRIANGLE) typeStr = "TRIANGLE";
    
    ss.str(""); ss << "Type: " << typeStr << " [T]";
    drawText(ss.str(), x + 10, py, 14, false, sf::Color::Yellow); py += 20;

    ss.str(""); ss << "Grid: " << gridW << "x" << gridH;
    drawText(ss.str(), x + 10, py, 14); py += 20;

    ss.str(""); ss << "Bundles: " << bundles.size();
    drawText(ss.str(), x + 10, py, 14); py += 20;
    
    y += 160;
    drawPanel(x, y, sidebarWidth - 40, 180); 
    py = y + 10;
    drawText("SETTINGS", x + 10, py, 16, false, colorAccent);
    for(auto& inp : inputs) inp->draw(window, font);

    y += 200;
    
    if (currentState != INIT) {
        drawPanel(x, y, sidebarWidth - 40, 80);
        py = y + 10;
        drawText("METRICS", x + 10, py, 16, false, colorAccent); py += 20;
        
        ss.str(""); ss << "Solver: " << (lastSolverName.empty() ? "N/A" : lastSolverName);
        drawText(ss.str(), x + 10, py, 14); py += 20;

        ss.str(""); ss << std::fixed << std::setprecision(4) << "Time: " << lastDuration << "s";
        drawText(ss.str(), x + 10, py, 14);
        
         if (currentState == SOLVED) {
             py += 20;
             ss.str(""); ss << "Area: " << (int)lastScore;
             drawText(ss.str(), x + 10, py, 14);
         }
    }
    y += 100;

    drawPanel(x, y, sidebarWidth - 40, 120);
    py = y + 10;
    drawText("CONTROLS", x + 10, py, 16, false, colorAccent); py += 20;
    
    std::vector<std::string> controls = {
        "[Enter] Apply & Regen",
        "[T] Switch Grid Type",
        "[G] Generate New",
        "[D] Solve DLX",
        "[S] Solve GRASP"
    };
    
    for (const auto& c : controls) {
        drawText(c, x + 10, py, 14, false, colorTextDim);
        py += 20;
    }
}

void App::drawPanel(float x, float y, float w, float h) {
    sf::RectangleShape rect(sf::Vector2f(w, h));
    rect.setPosition({x, y});
    rect.setFillColor(sf::Color(45, 45, 50));
    rect.setOutlineColor(sf::Color(60, 60, 60));
    rect.setOutlineThickness(1.0f);
    window.draw(rect);
}

void App::drawText(const std::string& str, float x, float y, unsigned int size, bool centered, sf::Color color) {
    if (!fontLoaded) return;
    sf::Text text(font, str, size);
    text.setFillColor(color);
    if (centered) {
        sf::FloatRect bounds = text.getLocalBounds();
        text.setOrigin({bounds.position.x + bounds.size.x / 2.0f, bounds.position.y + bounds.size.y / 2.0f});
        text.setPosition({x, y});
    } else {
        text.setPosition({x, y});
    }
    window.draw(text);
}
