#include "ui/InputBox.hpp"
#include <iostream>

InputBox::InputBox(float x, float y, float w, float h, const std::string& lbl, int* val, const sf::Font& font) 
    : textObj(font) 
{
    rect.setPosition({x, y});
    rect.setSize({w, h});
    rect.setFillColor(sf::Color(50, 50, 55));
    rect.setOutlineColor(sf::Color(100, 100, 100));
    rect.setOutlineThickness(1.0f);
    
    linkedValue = val;
    value = std::to_string(*val);
    label = lbl;
    
    textObj.setCharacterSize(14);
    textObj.setFillColor(sf::Color::White);
    textObj.setPosition({x + 5, y + 5});
    textObj.setString(value);
}

void InputBox::handleEvent(const sf::Event& event) {
    if (!isFocused) return;
    
    if (const auto* textEvent = event.getIf<sf::Event::TextEntered>()) {
        if (textEvent->unicode == 8) { // Backspace
            if (!value.empty()) value.pop_back();
        }
        else if (textEvent->unicode >= '0' && textEvent->unicode <= '9') {
            if (value.length() < 5) {
                value += static_cast<char>(textEvent->unicode);
            }
        }
        try {
            if (!value.empty()) *linkedValue = std::stoi(value);
        } catch(...) {}
        textObj.setString(value);
    }
}

void InputBox::update() {
    if (!isFocused && linkedValue) {
            value = std::to_string(*linkedValue);
            textObj.setString(value);
    }
}

void InputBox::draw(sf::RenderWindow& window, const sf::Font& font) {
    rect.setOutlineColor(isFocused ? sf::Color(70, 130, 180) : sf::Color(100, 100, 100));
    window.draw(rect);
    window.draw(textObj);
    
    sf::Text lblText(font, label, 12);
    lblText.setFillColor(sf::Color(180, 180, 180));
    lblText.setPosition({rect.getPosition().x, rect.getPosition().y - 18});
    window.draw(lblText);
}

bool InputBox::contains(float x, float y) {
    return rect.getGlobalBounds().contains({x, y});
}
