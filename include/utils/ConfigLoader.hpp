#pragma once
#include <fstream>
#include <string>
#include <nlohmann/json.hpp>
#include "../generators.h"
#include <iostream>

using json = nlohmann::json;

class ConfigLoader {
public:
    static GeneratorConfig load(const std::string& filename) {
        GeneratorConfig cfg;
        // Значения по умолчанию
        cfg.width = 10;
        cfg.height = 10;
        cfg.min_shape_size = 3;
        cfg.max_shape_size = 5;
        cfg.min_bundle_area = 15;
        cfg.max_bundle_area = 25;
        cfg.grid_type = GridType::SQUARE;

        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Config file not found: " << filename << ". Using defaults." << std::endl;
            return cfg;
        }

        try {
            json j;
            file >> j;
            
            if(j.contains("width")) cfg.width = j["width"];
            if(j.contains("height")) cfg.height = j["height"];
            if(j.contains("min_shape_size")) cfg.min_shape_size = j["min_shape_size"];
            if(j.contains("max_shape_size")) cfg.max_shape_size = j["max_shape_size"];
            if(j.contains("min_bundle_area")) cfg.min_bundle_area = j["min_bundle_area"];
            if(j.contains("max_bundle_area")) cfg.max_bundle_area = j["max_bundle_area"];
            if(j.contains("grid_type")) cfg.grid_type = (GridType)j["grid_type"];
        } catch (const std::exception& e) {
            std::cerr << "Error parsing config: " << e.what() << ". Using defaults for missing fields." << std::endl;
        }
        
        return cfg;
    }
};
