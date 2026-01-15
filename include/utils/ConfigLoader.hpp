#pragma once
#include <fstream>
#include <sstream>
#include <map>
#include <string>
#include "../generators.h"

class ConfigLoader {
public:
    static GeneratorConfig load(const std::string& filename) {
        GeneratorConfig cfg;
        std::ifstream file(filename);
        if (!file.is_open()) {
            // Defaults if file missing
            return cfg; 
        }

        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::stringstream ss(line);
            std::string key;
            if (std::getline(ss, key, '=')) {
                std::string valStr;
                std::getline(ss, valStr);
                int val = std::stoi(valStr);

                if (key == "width") cfg.width = val;
                else if (key == "height") cfg.height = val;
                else if (key == "min_shape_size") cfg.min_shape_size = val;
                else if (key == "max_shape_size") cfg.max_shape_size = val;
                else if (key == "min_bundle_area") cfg.min_bundle_area = val;
                else if (key == "max_bundle_area") cfg.max_bundle_area = val;
                else if (key == "grid_type") cfg.grid_type = (GridType)val;
            }
        }
        return cfg;
    }
};
