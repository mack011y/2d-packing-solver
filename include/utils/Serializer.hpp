#pragma once
#include "../core.hpp"
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class Serializer {
public:
    // Вспомогательный метод для сериализации топологии графа (Сетки или Фигуры)
    // Сохраняет список узлов и их связей (портов)
    template <typename T>
    static json serialize_graph_topology(const Graph<T>& graph) {
        json j_nodes = json::array();
        for (const auto& node : graph.get_nodes()) {
            std::vector<int> ports;
            for(int p=0; p < graph.get_max_ports(); ++p) {
                ports.push_back(node.get_neighbor(p));
            }
            
            j_nodes.push_back({
                {"id", node.get_id()},
                {"ports", ports}
            });
        }
        return j_nodes;
    }

    // Сохранение состояния задачи (сетка + бандлы) в JSON файл
    static void save_json(const std::string& filename, std::shared_ptr<Grid> grid, const std::vector<Bundle>& bundles) {
        json j;
        
        // 1. Информация о сетке
        j["grid"] = {
            {"width", grid->get_width()},
            {"height", grid->get_height()},
            {"type", (int)grid->get_type()},
            {"max_ports", grid->get_max_ports()}
        };

        // 2. Клетки (Узлы) с данными и топологией
        json cells = json::array();
        for (const auto& node : grid->get_nodes()) {
            std::vector<int> ports;
            for(int p=0; p < grid->get_max_ports(); ++p) {
                ports.push_back(node.get_neighbor(p));
            }

            cells.push_back({
                {"id", node.get_id()},
                {"x", node.get_data().x},
                {"y", node.get_data().y},
                {"bundle_id", node.get_data().bundle_id}, // ID бандла, если клетка занята
                {"figure_id", node.get_data().figure_id}, // ID конкретной фигуры
                {"ports", ports}
            });
        }
        j["cells"] = cells;

        // 3. Бандлы (Наборы фигур)
        json j_bundles = json::array();
        for(const auto& b : bundles) {
            json j_b;
            j_b["id"] = b.get_id();
            j_b["color"] = {b.get_color().r, b.get_color().g, b.get_color().b};
            j_b["area"] = b.get_total_area();
            
            json j_shapes = json::array();
            for(const auto& s : b.get_shapes()) {
                json j_s;
                j_s["name"] = s->name;
                j_s["size"] = s->size();
                j_s["max_ports"] = s->get_max_ports();
                // Сериализуем топологию самой фигуры (важно для сложных форм)
                j_s["topology"] = serialize_graph_topology(*s);
                j_shapes.push_back(j_s);
            }
            j_b["shapes"] = j_shapes;
            
            j_bundles.push_back(j_b);
        }
        j["bundles"] = j_bundles;

        std::ofstream out(filename);
        if (out.is_open()) {
            out << j.dump(4); // Красивый вывод с отступами
            std::cout << "Data saved to " << filename << std::endl;
        } else {
            std::cerr << "Failed to open output file: " << filename << std::endl;
        }
    }

    // Загрузка состояния задачи из JSON файла
    static std::pair<std::shared_ptr<Grid>, std::vector<Bundle>> load_json(const std::string& filename) {
        std::ifstream in(filename);
        if (!in.is_open()) {
            std::cerr << "Failed to open input file: " << filename << std::endl;
            return {nullptr, {}};
        }

        json j;
        in >> j;

        // 1. Восстановление Сетки
        int w = j["grid"]["width"];
        int h = j["grid"]["height"];
        int t = j["grid"]["type"];
        auto grid = std::make_shared<Grid>(w, h, (GridType)t);

        // 2. Создание узлов (Cells)
        for(const auto& cell : j["cells"]) {
             GridCellData data(cell["x"], cell["y"]);
             if(cell.contains("bundle_id")) data.bundle_id = cell["bundle_id"];
             if(cell.contains("figure_id")) data.figure_id = cell["figure_id"];
             grid->add_node(data);
        }

        // Восстановление связей (Ports)
        for(const auto& cell : j["cells"]) {
            int u = cell["id"];
            if (cell.contains("ports")) {
                const auto& ports = cell["ports"];
                for(int p=0; p < (int)ports.size(); ++p) {
                    int v = ports[p];
                    if (v != -1) {
                        grid->add_directed_edge(u, v, p);
                    }
                }
            } else if (cell.contains("neighbors")) {
                // Поддержка старого формата (на всякий случай)
                int p = 0;
                for(int v : cell["neighbors"]) {
                    grid->add_directed_edge(u, v, p++); 
                }
            }
        }

        // 3. Восстановление Бандлов
        std::vector<Bundle> bundles;
        if(j.contains("bundles")) {
            for(const auto& b_json : j["bundles"]) {
                Color c = {255, 255, 255};
                if(b_json.contains("color")) {
                    c = {b_json["color"][0], b_json["color"][1], b_json["color"][2]};
                }
                int id = b_json["id"];
                
                std::vector<std::shared_ptr<Figure>> shapes;
                if(b_json.contains("shapes")) {
                    for(const auto& s_json : b_json["shapes"]) {
                        std::string name = s_json["name"];
                        int mp = s_json.contains("max_ports") ? (int)s_json["max_ports"] : grid->get_max_ports();
                        
                        auto fig = std::make_shared<Figure>(name, mp);
                        
                        if(s_json.contains("topology")) {
                            // Создаем узлы фигуры
                            for(const auto& node_json : s_json["topology"]) {
                                fig->add_node(); // ID авто-инкрементируется: 0, 1, 2...
                            }
                            // Восстанавливаем связи внутри фигуры
                            for(const auto& node_json : s_json["topology"]) {
                                int u = node_json["id"];
                                const auto& ports = node_json["ports"];
                                for(int p=0; p < (int)ports.size(); ++p) {
                                    int v = ports[p];
                                    if (v != -1) {
                                        fig->add_directed_edge(u, v, p);
                                    }
                                }
                            }
                        }
                        shapes.push_back(fig);
                    }
                }
                
                Bundle b(id, shapes, c); 
                bundles.push_back(b);
            }
        }

        return {grid, bundles};
    }
};
