#pragma once
#include <fstream>
#include <string>
#include <vector>
#include <cmath>
#include <sstream>
#include <iomanip>
#include "../core.hpp"

class SVGExporter {
public:
    static void save(const std::string& filename, std::shared_ptr<Grid> grid) {
        std::ofstream file(filename);
        if (!file.is_open()) return;

        int width = grid->get_width();
        int height = grid->get_height();
        
        // Размер ячейки и отступы
        double size = 30.0;
        double margin = 20.0;
        
        // Вычисляем размер холста
        double canvas_w = width * size + margin * 2;
        double canvas_h = height * size + margin * 2;

        // Корректировка для гексов (примерная)
        if (grid->get_type() == GridType::HEXAGON) {
             canvas_w = width * size * 1.0 + margin * 2; // Упрощено
             canvas_h = height * size * 0.86 + margin * 2;
        }

        file << "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" "
             << "width=\"" << canvas_w << "\" height=\"" << canvas_h << "\">\n";

        // Фон
        file << "<rect width=\"100%\" height=\"100%\" fill=\"#f8f9fa\" />\n";

        for (const auto& node : grid->get_nodes()) {
            const auto& data = node.get_data();
            std::string color = "#ffffff";
            std::string stroke = "#dee2e6";
            
            // Если занято бандлом - генерируем цвет
            if (data.bundle_id != -1) {
                color = generate_color(data.bundle_id);
                stroke = "#000000";
            } else if (data.figure_id == -2) { // Например, препятствие
                color = "#343a40"; 
            }

            draw_cell(file, grid->get_type(), data.x, data.y, size, margin, color, stroke, data.bundle_id);
        }

        file << "</svg>";
        file.close();
        std::cout << "Saved visualization to " << filename << std::endl;
    }

private:
    static std::string generate_color(int id) {
        // Простой хеш для стабильных цветов
        unsigned int hash = id;
        hash = ((hash >> 16) ^ hash) * 0x45d9f3b;
        hash = ((hash >> 16) ^ hash) * 0x45d9f3b;
        hash = (hash >> 16) ^ hash;
        
        int r = (hash & 0xFF);
        int g = ((hash >> 8) & 0xFF);
        int b = ((hash >> 16) & 0xFF);
        
        // Делаем цвета пастельными/светлыми, чтобы текст читался
        r = (r + 255) / 2;
        g = (g + 255) / 2;
        b = (b + 255) / 2;

        std::stringstream ss;
        ss << "rgb(" << r << "," << g << "," << b << ")";
        return ss.str();
    }

    static void draw_cell(std::ofstream& file, GridType type, int gx, int gy, double size, double margin, 
                         std::string fill, std::string stroke, int label_id) {
        
        if (type == GridType::SQUARE) {
            double x = margin + gx * size;
            double y = margin + gy * size;
            
            file << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << size << "\" height=\"" << size 
                 << "\" fill=\"" << fill << "\" stroke=\"" << stroke << "\" stroke-width=\"1\" />\n";
                 
            if (label_id != -1) {
                file << "<text x=\"" << x + size/2 << "\" y=\"" << y + size/2 + 5 << "\" "
                     << "text-anchor=\"middle\" fill=\"black\" font-family=\"Arial\" font-size=\"10\">" 
                     << label_id << "</text>\n";
            }
        }
        else if (type == GridType::HEXAGON) {
            // "Pointy topped" hexes stored in offset coordinates
            double w = size; 
            double h = size * 0.866025; // sin(60)
            
            // Смещение для нечетных строк
            double x_offset = (gy % 2) * (w / 2.0);
            
            double cx = margin + gx * w + x_offset + w/2;
            double cy = margin + gy * h + h/2;
            double r = size / 1.8; // Радиус

            file << "<polygon points=\"";
            for (int i = 0; i < 6; i++) {
                double angle_deg = 30 + 60 * i;
                double angle_rad = angle_deg * M_PI / 180.0;
                file << (cx + r * cos(angle_rad)) << "," << (cy + r * sin(angle_rad)) << " ";
            }
            file << "\" fill=\"" << fill << "\" stroke=\"" << stroke << "\" stroke-width=\"1\" />\n";
        }
        // TODO: Добавить Triangle при необходимости
    }
};
