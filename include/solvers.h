#pragma once
#include "core.hpp"
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <functional>

// Base Solver Interface
class Solver {
public:
    std::shared_ptr<Grid> graph; // Теперь работаем с Grid
    std::vector<Bundle> bundles;
    std::vector<int> placed_bundles; 
    
    Solver(std::shared_ptr<Grid> g, const std::vector<Bundle>& b) 
        : graph(g), bundles(b) {}
    
    virtual float solve() = 0;
    virtual ~Solver() = default;
};

class DLXSolver : public Solver {
public:
    DLXSolver(std::shared_ptr<Grid> g, const std::vector<Bundle>& b) 
        : Solver(g, b) {}
        
    float solve() override;
    
private:
    struct MatrixRow {
        int id;
        std::set<std::string> cols;
        // Данные для восстановления:
        std::shared_ptr<Figure> figure; // Какой подграф
        int anchor_grid_id;             // В каком узле сетки якорь (узел 0 фигуры)
        int rotation;                   // Поворот (смещение портов)
        int bundle_id;
    };
    
    std::map<int, MatrixRow> rows;
    std::map<std::string, std::set<int>> cols;
    
    bool search(std::vector<int>& solution);
    void apply_solution(const std::vector<int>& solution);
};

class GRASPSolver : public Solver {
public:
    int max_iterations = 100;
    float alpha = 0.8f;

    GRASPSolver(std::shared_ptr<Grid> g, const std::vector<Bundle>& b) 
        : Solver(g, b) {}
        
    float solve() override;
    
private:
    struct SolutionState {
        float score;
        std::map<int, int> node_allocations;
        std::map<int, int> node_figure_ids; // Added field
        std::vector<int> placed_bundle_ids;
    };

    float run_construction_phase(SolutionState& state);
};
