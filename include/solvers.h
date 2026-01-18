#pragma once
#include "core.hpp"
#include <vector>
#include <memory>
#include <random> 

struct SolverConfig {
    int max_iterations = 50;
    float alpha = 0.8f;
    bool verbose = false;
    double max_time_seconds = 0.0;
};

struct SolverResult {
    float score;
    std::vector<int> placed_bundles;
};

class GRASPSolver {
public:
    std::shared_ptr<Grid> graph;
    std::vector<Bundle> bundles;
    std::vector<int> placed_bundles;
    SolverConfig config;
    
    GRASPSolver(const Puzzle& p, SolverConfig cfg = SolverConfig()) 
        : graph(p.get_grid()), bundles(p.get_bundles()), config(cfg) {}
        
    SolverResult solve();
    
private:
    struct SolutionState {
        float score;
        std::vector<int> node_allocations;
        std::vector<int> node_figure_ids;
        std::vector<int> placed_bundle_ids;
    };

    struct SinglePlacement {
        std::shared_ptr<Figure> figure; 
        int anchor;                     
        int rotation;                   
        std::vector<int> footprint;     
        int score;                      
    };

    SolutionState run_construction_phase();
    
    int calculate_placement_score(const std::vector<int>& footprint, const std::vector<char>& occupied_mask);
    
    bool place_shapes_recursive(
        int shape_idx, 
        const std::vector<std::shared_ptr<Figure>>& shapes, 
        std::vector<char>& current_occupied_mask, 
        std::vector<SinglePlacement>& out_placements,
        std::mt19937& rng
    );
};
