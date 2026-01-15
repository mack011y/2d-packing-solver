#pragma once
#include <SFML/Graphics.hpp>
#include <string>

class InputBox {
public:
    sf::RectangleShape rect;
    sf::Text textObj;
    std::string value;
    bool isFocused = false;
    std::string label;
    int* linkedValue;

    InputBox(float x, float y, float w, float h, const std::string& lbl, int* val, const sf::Font& font);

    void handleEvent(const sf::Event& event);
    void update();
    void draw(sf::RenderWindow& window, const sf::Font& font);
    bool contains(float x, float y);
};
