#pragma once
#include "core.hpp"
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <functional>
#include <random> // Added for rng in header
#include "heuristics.h" // Подключаем модуль эвристик

// --- Shared Helpers ---
// Пытается наложить граф фигуры (figure) на граф сетки (grid).
// Возвращает список занятых узлов сетки или пустой список при неудаче.
std::vector<int> get_embedding(std::shared_ptr<Grid> grid, int anchor_id, std::shared_ptr<Figure> figure, int rotation);


// Base Solver Interface
// Базовый класс для всех решателей.
// Предоставляет общий интерфейс и хранит ссылки на граф (поле) и бандлы (фигуры).
class Solver {
public:
    std::shared_ptr<Grid> graph; // Поле для замощения
    std::vector<Bundle> bundles; // Набор фигур (в бандлах)
    std::vector<int> placed_bundles; // Результат: список ID успешно размещенных бандлов
    
    Solver(std::shared_ptr<Grid> g, const std::vector<Bundle>& b) 
        : graph(g), bundles(b) {}
    
    // Чистая виртуальная функция, которую должен реализовать каждый конкретный алгоритм.
    // Возвращает оценку качества решения (Score).
    virtual float solve() = 0;
    virtual ~Solver() = default;
};

// DLX Solver (Exact Cover)
// Реализация алгоритма "Танцующие ссылки" (Dancing Links) Дональда Кнута.
// Преобразует задачу в матрицу точного покрытия.
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

// GRASP Solver
// Greedy Randomized Adaptive Search Procedure.
// Строит решение жадно, но с элементами случайности, чтобы исследовать разные варианты.
class GRASPSolver : public Solver {
public:
    int max_iterations = 5; // Было 100, ставим 5 для теста 60x60
    float alpha = 0.8f; // Параметр жадности (1.0 = чисто жадный, 0.0 = чисто случайный)

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

// Simulated Annealing Solver (Permutation-based)
// Имитация отжига. Оптимизирует порядок (перестановку) и эвристики размещения фигур.
class SimulatedAnnealingSolver : public Solver {
public:
    int max_iterations = 20000;
    float initial_temp = 5000.0f;
    float cooling_rate = 0.9995f;

    SimulatedAnnealingSolver(std::shared_ptr<Grid> g, const std::vector<Bundle>& b) 
        : Solver(g, b) {}
        
    float solve() override;
    
private:
    struct Gene {
        int bundle_id;
        int heuristic;
    };
    
    struct State {
        std::vector<Gene> sequence; // Permutation of all bundles
        float energy; // Cached energy (negative score)
        // Для кэширования результата (чтобы не строить заново для визуализации)
        // В реальном SA это не нужно хранить в стейте, но мы сохраним для финала
    };

    struct PlacedShape {
        int anchor_id;
        int rotation;
        std::shared_ptr<Figure> figure;
        std::vector<int> footprint;
    };

    // Строит решение из последовательности и возвращает "Энергию" (отрицательный score)
    float evaluate_sequence(const std::vector<Gene>& sequence, std::map<int, std::vector<PlacedShape>>* out_placement = nullptr);
    
    State get_neighbor(const State& current, std::mt19937& rng);
};

// Genetic Algorithm Solver (Coordinate-based)
// Классический генетический алгоритм. Особь кодирует координаты размещения фигур.
class GeneticAlgorithmSolver : public Solver {
public:
    int population_size = 50;
    int generations = 5; // Было 10, ставим 5 для теста
    float mutation_rate = 0.1f;
    int elite_count = 5;

    GeneticAlgorithmSolver(std::shared_ptr<Grid> g, const std::vector<Bundle>& b) 
        : Solver(g, b) {}
        
    float solve() override;

private:
    struct PlacedShape {
        int anchor_id;
        int rotation;
        std::shared_ptr<Figure> figure;
        std::vector<int> footprint;
    };

    struct Individual {
        // bundle_id -> list of placed shapes
        std::map<int, std::vector<PlacedShape>> active_bundles;
        // set of occupied grid node IDs (for fast collision check)
        // OPTIMIZATION: Replaced std::set with std::vector<char> for O(1) access
        std::vector<char> occupied_nodes; 
        float fitness;
    };

    float calculate_fitness(const Individual& ind);
    Individual create_random_individual(std::mt19937& rng);
    Individual crossover(const Individual& p1, const Individual& p2, std::mt19937& rng);
    void mutate(Individual& ind, std::mt19937& rng);
    
    // Helper to try add a bundle to an individual
    bool try_add_bundle(Individual& ind, int bundle_id, std::mt19937& rng);
};

    // Genetic Permutation Solver (Order-based with Hyper-Heuristics)
    // Новый ГА. Особь кодирует ПОРЯДОК (перестановку) фигур и ЭВРИСТИКУ размещения.
    // Размещение выполняется "Строителем" (Builder), который применяет выбранную эвристику.
    class GeneticPermutationSolver : public Solver {
    public:
        int population_size = 10;
        int generations = 20;
        float mutation_rate = 0.4f;
        int elite_count = 2;

        GeneticPermutationSolver(std::shared_ptr<Grid> g, const std::vector<Bundle>& b) 
            : Solver(g, b) {}
            
        float solve() override;

    private:
        // Enum HeuristicType теперь берется из heuristics.h

        struct Gene {
            int bundle_id;
            int heuristic; // Теперь это int, который кастится к HeuristicType
        };

        struct PlacedShape {
            int anchor_id;
            int rotation;
            std::shared_ptr<Figure> figure;
            std::vector<int> footprint;
        };

        struct PlacementResult {
            float score;
            std::map<int, std::vector<PlacedShape>> active_bundles; 
            // OPTIMIZATION: Used vector for fast lookup
            std::vector<char> occupied_nodes;
        };

        struct Individual {
            std::vector<Gene> chromosome; 
            float fitness;
        };

        PlacementResult build_solution(const std::vector<Gene>& chromosome);
        Individual create_random_individual(std::mt19937& rng);
        Individual crossover(const Individual& p1, const Individual& p2, std::mt19937& rng);
        void mutate(Individual& ind, std::mt19937& rng);
    };
