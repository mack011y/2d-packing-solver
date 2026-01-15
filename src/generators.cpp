#include "generators.h"
#include <random>
#include <algorithm>
#include <set>
#include <iostream>
#include <queue>
#include <cmath>

// --- Static Helpers ---

static void hsv_to_rgb(float h, float s, float v, int& r, int& g, int& b) {
    int i = int(h * 6);
    float f = h * 6 - i;
    float p = v * (1 - s);
    float q = v * (1 - f * s);
    float t = v * (1 - (1 - f) * s);

    float rf, gf, bf;
    switch (i % 6) {
        case 0: rf = v; gf = t; bf = p; break;
        case 1: rf = q; gf = v; bf = p; break;
        case 2: rf = p; gf = v; bf = t; break;
        case 3: rf = p; gf = q; bf = v; break;
        case 4: rf = t; gf = p; bf = v; break;
        case 5: rf = v; gf = p; bf = q; break;
    }
    r = int(rf * 255);
    g = int(gf * 255);
    b = int(bf * 255);
}

// --- PuzzleGenerator Implementation ---

PuzzleGenerator::PuzzleGenerator(const GeneratorConfig& cfg) : config(cfg) {}

std::shared_ptr<Grid> PuzzleGenerator::create_square_grid() {
    auto g = std::make_shared<Grid>(config.width, config.height, GridType::SQUARE);
    
    // Nodes
    for(int y=0; y<config.height; ++y) {
        for(int x=0; x<config.width; ++x) {
            g->add_node(GridCellData(x, y));
        }
    }
    
    // Edges
    for(int y=0; y<config.height; ++y) {
        for(int x=0; x<config.width; ++x) {
            int id = y*config.width + x;
            if(x < config.width - 1) g->add_edge(id, y*config.width+(x+1), 1, 3); // R <-> L
            if(y < config.height - 1) g->add_edge(id, (y+1)*config.width+x, 2, 0); // B <-> T
        }
    }
    return g;
}

std::shared_ptr<Grid> PuzzleGenerator::create_hex_grid() {
    auto g = std::make_shared<Grid>(config.width, config.height, GridType::HEXAGON);

    for(int y=0; y<config.height; ++y) {
        for(int x=0; x<config.width; ++x) {
            g->add_node(GridCellData(x, y));
        }
    }

    // Pointy-topped Hexagons with Odd-R horizontal layout
    int er_dx[] = {0, 1, 0, -1, -1, -1};
    int er_dy[] = {-1, 0, 1, 1, 0, -1};
    int or_dx[] = {1, 1, 1, 0, -1, 0};
    int or_dy[] = {-1, 0, 1, 1, 0, -1};

    for(int y=0; y<config.height; ++y) {
        for(int x=0; x<config.width; ++x) {
            int id = y*config.width + x;
            int* dx = (y % 2 == 0) ? er_dx : or_dx;
            int* dy = (y % 2 == 0) ? er_dy : or_dy;

            for(int p=0; p<6; ++p) {
                int nx = x + dx[p];
                int ny = y + dy[p];
                
                if (nx >= 0 && nx < config.width && ny >= 0 && ny < config.height) {
                    int nid = ny * config.width + nx;
                    g->add_edge(id, nid, p, (p+3)%6);
                }
            }
        }
    }
    return g;
}

std::shared_ptr<Grid> PuzzleGenerator::create_triangle_grid() {
    auto g = std::make_shared<Grid>(config.width, config.height, GridType::TRIANGLE);
    
    for(int y=0; y<config.height; ++y) {
        for(int x=0; x<config.width; ++x) {
            g->add_node(GridCellData(x, y));
        }
    }

    for(int y=0; y<config.height; ++y) {
        for(int x=0; x<config.width; ++x) {
            int id = y*config.width + x;
            bool is_up = ((x + y) % 2 == 0);
            
            if (x < config.width - 1) g->add_edge(id, y*config.width + (x+1), 0, 1); // R <-> L
            
            if (is_up) {
                if (y < config.height - 1) g->add_edge(id, (y+1)*config.width + x, 2, 2); 
            } else {
                if (y > 0) g->add_edge(id, (y-1)*config.width + x, 2, 2); 
            }
        }
    }
    return g;
}

std::shared_ptr<Figure> PuzzleGenerator::subset_to_figure(std::string name, const std::vector<int>& node_ids, std::shared_ptr<Grid> grid) {
    // 1. Create empty Figure
    auto fig = std::make_shared<Figure>(name, grid->get_max_ports());
    
    // 2. Map: Grid Node ID -> Figure Node ID
    std::map<int, int> grid_to_fig;
    
    // Add nodes
    for (int gid : node_ids) {
        int fid = fig->add_node(); // Add generic node
        grid_to_fig[gid] = fid;
    }
    
    // Add edges (copy topology from grid)
    for (int gid : node_ids) {
        const auto& g_node = grid->get_node(gid);
        int fid = grid_to_fig[gid];
        
        for (int p=0; p < grid->get_max_ports(); ++p) {
            int neighbor_gid = g_node.get_neighbor(p);
            
            // If neighbor is part of the subset
            if (neighbor_gid != -1 && grid_to_fig.count(neighbor_gid)) {
                int neighbor_fid = grid_to_fig[neighbor_gid];
                fig->add_directed_edge(fid, neighbor_fid, p);
            }
        }
    }
    
    return fig;
}

std::pair<std::vector<Bundle>, std::map<int, int>> PuzzleGenerator::generate(std::shared_ptr<Grid>& out_grid) {
    piece_counter = 0;

    // 1. Create Grid
    if (config.grid_type == GridType::HEXAGON) {
        out_grid = create_hex_grid();
    } else if (config.grid_type == GridType::TRIANGLE) {
        out_grid = create_triangle_grid();
    } else {
        out_grid = create_square_grid();
    }

    std::set<int> available_nodes;
    for(size_t i=0; i<out_grid->size(); ++i) {
        available_nodes.insert(i);
    }

    struct TempShape {
        std::shared_ptr<Figure> graph;
        std::vector<int> cells;
        int area;
    };
    std::vector<TempShape> shapes_data;
    
    std::random_device rd;
    std::mt19937 gen(rd());

    // 2. Grow shapes (Region Growing)
    while(!available_nodes.empty()) {
        std::vector<int> av_vec(available_nodes.begin(), available_nodes.end());
        std::uniform_int_distribution<> dis(0, av_vec.size()-1);
        int start = av_vec[dis(gen)];

        std::set<int> current_shape = {start};
        std::vector<int> growth = {start};
        
        std::uniform_int_distribution<> size_dist(config.min_shape_size, config.max_shape_size);
        int target_size = size_dist(gen);

        while(current_shape.size() < target_size && !growth.empty()) {
            std::uniform_int_distribution<> g_dist(0, growth.size()-1);
            int g_idx = g_dist(gen);
            int grow_from = growth[g_idx];

            std::vector<int> valid_neighbors;
            for(int p=0; p < out_grid->get_max_ports(); ++p) {
                int n = out_grid->get_node(grow_from).get_neighbor(p);
                if (n != -1 && available_nodes.count(n) && current_shape.find(n) == current_shape.end()) {
                    valid_neighbors.push_back(n);
                }
            }

            if(valid_neighbors.empty()) {
                growth.erase(growth.begin() + g_idx);
                continue;
            }

            std::uniform_int_distribution<> n_dist(0, valid_neighbors.size()-1);
            int next = valid_neighbors[n_dist(gen)];
            current_shape.insert(next);
            growth.push_back(next);
        }

        for(int id : current_shape) available_nodes.erase(id);

        std::vector<int> cell_list(current_shape.begin(), current_shape.end());
        
        // Push raw shape, will convert later
        shapes_data.push_back({nullptr, cell_list, (int)current_shape.size()});
    }

    // 2.5 Merge small shapes
    auto rebuild_lookup = [&](std::vector<int>& lookup) {
        lookup.assign(out_grid->size(), -1);
        for(size_t i=0; i<shapes_data.size(); ++i) {
            for(int cid : shapes_data[i].cells) {
                lookup[cid] = i;
            }
        }
    };
    
    std::vector<int> cell_to_shape_idx(out_grid->size(), -1);
    rebuild_lookup(cell_to_shape_idx);
    
    bool merged = true;
    while(merged) {
        merged = false;
        for(size_t i=0; i<shapes_data.size(); ++i) {
            if (shapes_data[i].cells.empty()) continue; 
            
            if (shapes_data[i].area < config.min_shape_size) {
                std::set<int> neighbor_shapes;
                for(int cid : shapes_data[i].cells) {
                    const auto& node = out_grid->get_node(cid);
                    for(int neighbor_cid : node.get_all_neighbors()) {
                        if (neighbor_cid != -1) {
                            int neighbor_shape_idx = cell_to_shape_idx[neighbor_cid];
                            if (neighbor_shape_idx != -1 && neighbor_shape_idx != (int)i) {
                                neighbor_shapes.insert(neighbor_shape_idx);
                            }
                        }
                    }
                }
                
                if (!neighbor_shapes.empty()) {
                    std::vector<int> neighbors_vec(neighbor_shapes.begin(), neighbor_shapes.end());
                    std::uniform_int_distribution<> ndist(0, neighbors_vec.size()-1);
                    int target_idx = neighbors_vec[ndist(gen)];
                    
                    auto& target = shapes_data[target_idx];
                    auto& src = shapes_data[i];
                    
                    target.cells.insert(target.cells.end(), src.cells.begin(), src.cells.end());
                    target.area += src.area;
                    
                    for(int cid : src.cells) cell_to_shape_idx[cid] = target_idx;
                    
                    src.cells.clear();
                    src.area = 0;
                    merged = true;
                }
            }
        }
        
        if (merged) {
            std::vector<TempShape> new_shapes;
            for(const auto& s : shapes_data) {
                if (!s.cells.empty()) new_shapes.push_back(s);
            }
            shapes_data = new_shapes;
            rebuild_lookup(cell_to_shape_idx);
        }
    }
    
    // Create Figure objects
    for(auto& shape : shapes_data) {
        auto fig = subset_to_figure("S_" + std::to_string(piece_counter), shape.cells, out_grid);
        shape.graph = fig;
        
        for(int cid : shape.cells) {
            out_grid->get_node(cid).get_data().figure_id = piece_counter;
        }
        piece_counter++;
    }

    // 3. Group into Bundles (Area based)
    std::shuffle(shapes_data.begin(), shapes_data.end(), gen);
    
    std::vector<Bundle> bundles;
    std::map<int, int> solution_map;
    
    size_t idx = 0;
    int bundle_counter = 0;
    
    std::uniform_int_distribution<> area_dist(
        config.min_bundle_area, 
        config.max_bundle_area
    );

    while(idx < shapes_data.size()) {
        int target_area = area_dist(gen);
        int current_bundle_area = 0;
        std::vector<std::shared_ptr<Figure>> group_shapes;
        
        while(idx < shapes_data.size()) {
             auto& item = shapes_data[idx];
             
             if (current_bundle_area > 0 && current_bundle_area >= target_area) {
                 break; 
             }
             
             group_shapes.push_back(item.graph);
             current_bundle_area += item.area;
             
             for(int nid : item.cells) {
                 solution_map[nid] = bundle_counter;
             }
             
             idx++;
        }

        if (group_shapes.empty()) break; 

        int bid = bundle_counter;
        
        // Calc color
        float min_area_est = (float)config.min_bundle_area;
        float max_area_est = (float)config.max_bundle_area;
        float t = (current_bundle_area - min_area_est) / (max_area_est - min_area_est);
        if (t < 0.0f) t = 0.0f; else if (t > 1.0f) t = 1.0f;
        
        float h = (1.0f - t) * (120.0f / 360.0f);
        int cr, cg, cb;
        hsv_to_rgb(h, 1.0f, 1.0f, cr, cg, cb);

        Bundle new_bundle(bid, group_shapes, {cr, cg, cb});
        bundles.push_back(new_bundle);

        bundle_counter++;
    }
    
    // Post-Process Color by Rank
    struct BundleInfo { int index; size_t area; };
    std::vector<BundleInfo> b_infos;
    for(int i=0; i<bundles.size(); ++i) b_infos.push_back({i, bundles[i].get_total_area()});
    
    std::sort(b_infos.begin(), b_infos.end(), [](auto& a, auto& b){ return a.area < b.area; });
    
    for(int rank=0; rank < b_infos.size(); ++rank) {
        float t = 0.0f;
        if (b_infos.size() > 1) t = (float)rank / (float)(b_infos.size() - 1);
        float h = (1.0f - t) * (120.0f / 360.0f);
        int r, g, b;
        hsv_to_rgb(h, 1.0f, 1.0f, r, g, b);
        
        bundles[b_infos[rank].index].set_color({r, g, b});
    }
    
    return {bundles, solution_map};
}
