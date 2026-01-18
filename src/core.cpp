#include "core.hpp"
#include <vector>
#include <queue>


std::vector<int> Grid::get_embedding(std::shared_ptr<Figure> figure, int anchor_id, int rotation) const {
    if (figure->size() == 0) return {};
    
    std::vector<int> mapping(figure->size(), -1); 
    mapping[0] = anchor_id;
    
    std::vector<int> q = {0};
    std::vector<bool> visited(figure->size(), false);
    visited[0] = true;
    
    size_t head = 0;
    while(head < q.size()) {
        int u_fig = q[head++];
        int u_grid = mapping[u_fig];
        
        const auto& fig_node = figure->get_node(u_fig);
        
        for(size_t p = 0; p < figure->get_max_ports(); ++p) {
            int v_fig = fig_node.get_neighbor(p);
            if (v_fig == -1) continue;
            
            if (visited[v_fig]) {
                continue; 
            }
            
            size_t rot_port = (p + rotation) % this->get_max_ports();
            int v_grid = this->get_node(u_grid).get_neighbor(rot_port);
            
            if (v_grid == -1) return {}; 
            
            for(int m : mapping) if(m == v_grid) return {};
            
            mapping[v_fig] = v_grid;
            visited[v_fig] = true;
            q.push_back(v_fig);
        }
    }
    
    return mapping;
}

Bundle::Bundle(int id, std::vector<std::shared_ptr<Figure>> shapes, const Color& color)
    : id(id), shapes(std::move(shapes)), color(color) {
    recalculate_area();
}

void Bundle::recalculate_area() {
    total_area = 0;
    for(const auto& s : shapes) {
        total_area += s->size();
    }
}

Puzzle Puzzle::clone() const {
    auto new_grid = std::make_shared<Grid>(*grid); 
    return Puzzle(new_grid, bundles, name);
}

void Puzzle::clear_grid() {
    if (!grid) return;
    for (auto& node : grid->get_nodes()) {
        node.get_data().bundle_id = -1;
        node.get_data().figure_id = -1;
    }
}
