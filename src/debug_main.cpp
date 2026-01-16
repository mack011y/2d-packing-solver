#include <iostream>
#include <vector>
#include "utils/Serializer.hpp"

int main(int argc, char* argv[]) {
    if(argc < 2) return 1;
    
    auto [grid, bundles] = Serializer::load_json(argv[1]);
    
    std::cout << "Grid: " << grid->get_width() << "x" << grid->get_height() << std::endl;
    std::cout << "Nodes: " << grid->size() << std::endl;
    std::cout << "Max Ports: " << grid->get_max_ports() << std::endl;
    
    int total_neighbors = 0;
    int valid_neighbors = 0;
    for(const auto& node : grid->get_nodes()) {
        for(int n : node.get_all_neighbors()) {
            total_neighbors++;
            if (n != -1) valid_neighbors++;
        }
    }
    std::cout << "Total Slots: " << total_neighbors << std::endl;
    std::cout << "Valid Neighbors: " << valid_neighbors << std::endl;
    std::cout << "Avg Neighbors: " << (float)valid_neighbors / grid->size() << std::endl;
    
    std::cout << "--- Bundles ---" << std::endl;
    std::cout << "Count: " << bundles.size() << std::endl;
    for(const auto& b : bundles) {
        std::cout << "Bundle " << b.get_id() << ": " << b.get_shapes().size() << " shapes." << std::endl;
        for(const auto& s : b.get_shapes()) {
            std::cout << "  Shape " << s->name << ": size " << s->size() << std::endl;
            // Check shape connectivity
            int s_valid = 0;
            for(const auto& n : s->get_nodes()) {
                for(int nn : n.get_all_neighbors()) if(nn != -1) s_valid++;
            }
            std::cout << "    Edges: " << s_valid << std::endl;
        }
    }
    
    return 0;
}
