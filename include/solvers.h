#pragma once
#include "core.hpp"
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <functional>
#include <random> 
#include "heuristics.h" 

// --- Вспомогательные функции ---

// Пытается наложить граф фигуры (figure) на граф сетки (grid).
// Это "сердце" геометрических проверок всех алгоритмов.
// 
// Аргументы:
// - grid: указатель на сетку (большой граф)
// - anchor_id: ID узла сетки, к которому мы прикладываем "нулевой" узел фигуры (якорь)
// - figure: указатель на фигуру (малый граф)
// - rotation: целочисленный поворот (сдвиг портов: 0, 1, 2...). 
//
// Возвращает:
// - Список ID узлов сетки, которые займет фигура.
// - Пустой список, если фигура не влезает (выход за границы, самопересечение, несовпадение топологии).
std::vector<int> get_embedding(std::shared_ptr<Grid> grid, int anchor_id, std::shared_ptr<Figure> figure, int rotation);


// --- Configuration ---
struct SolverConfig {
    // GRASP
    int grasp_max_iterations = 50;
    float grasp_alpha = 0.8f;

    // Simulated Annealing
    int sa_max_iterations = 20000;
    float sa_initial_temp = 5000.0f;
    float sa_cooling_rate = 0.9995f;

    // Genetic Algorithms
    int ga_population_size = 50;
    int ga_generations = 50;
    float ga_mutation_rate = 0.1f;
    int ga_elite_count = 5;

    // Debug / Output
    bool verbose = false;
};

// --- Базовый Интерфейс Решателя (Solver) ---
// Все алгоритмы наследуются от этого класса.
class Solver {
public:
    std::shared_ptr<Grid> graph;     // Поле для замощения
    std::vector<Bundle> bundles;     // Полный список фигур ("инвентарь")
    std::vector<int> placed_bundles; // Результат: список ID бандлов, которые удалось разместить
    SolverConfig config;             // Конфигурация
    
    Solver(std::shared_ptr<Grid> g, const std::vector<Bundle>& b, SolverConfig cfg = SolverConfig()) 
        : graph(g), bundles(b), config(cfg) {}
    
    // Основной метод запуска. Должен быть реализован в наследниках.
    // Возвращает итоговый Score (обычно количество занятых клеток).
    virtual float solve() = 0;
    virtual ~Solver() = default;
};

// --- 1. DLX Solver (Exact Cover) ---
class DLXSolver : public Solver {
public:
    DLXSolver(std::shared_ptr<Grid> g, const std::vector<Bundle>& b, SolverConfig cfg = SolverConfig()) 
        : Solver(g, b, cfg) {}
        
    float solve() override;
    
private:
    struct MatrixRow {
        int id;
        std::set<std::string> cols;     // Столбцы, которые покрывает эта строка
        // Данные для восстановления решения:
        std::shared_ptr<Figure> figure; // Какую фигуру кладем
        int anchor_grid_id;             // В какой узел сетки
        int rotation;                   // С каким поворотом
        int bundle_id;
    };
    
    std::map<int, MatrixRow> rows;          // Строки матрицы
    std::map<std::string, std::set<int>> cols; // Столбцы матрицы
    
    bool search(std::vector<int>& solution); // Рекурсивный поиск
    void apply_solution(const std::vector<int>& solution);
};

// --- 2. GRASP Solver ---
class GRASPSolver : public Solver {
public:
    GRASPSolver(std::shared_ptr<Grid> g, const std::vector<Bundle>& b, SolverConfig cfg = SolverConfig()) 
        : Solver(g, b, cfg) {}
        
    float solve() override;
    
private:
    struct SolutionState {
        float score;
        std::map<int, int> node_allocations; // Карта занятых узлов (GridID -> BundleID)
        std::map<int, int> node_figure_ids;  // Карта фигур (GridID -> FigureID)
        std::vector<int> placed_bundle_ids;  // Список размещенных бандлов
    };

    float run_construction_phase(SolutionState& state);
};

// --- 3. Simulated Annealing Solver (Permutation-based) ---
class SimulatedAnnealingSolver : public Solver {
public:
    SimulatedAnnealingSolver(std::shared_ptr<Grid> g, const std::vector<Bundle>& b, SolverConfig cfg = SolverConfig()) 
        : Solver(g, b, cfg) {}
        
    float solve() override;
    
private:
    // "Ген" решения: какую фигуру взять и как (по какой стратегии) её положить
    struct Gene {
        int bundle_id;
        int heuristic; // ID эвристики (HeuristicType)
    };
    
    struct State {
        std::vector<Gene> sequence; // Перестановка (хромосома)
        float energy;               // Энергия состояния (отрицательный score)
    };

    // Структура для хранения результата размещения одной фигуры
    struct PlacedShape {
        int anchor_id;
        int rotation;
        std::shared_ptr<Figure> figure;
        std::vector<int> footprint; // Занятые клетки
    };

    // Строит полное решение из последовательности генов и возвращает энергию
    float evaluate_sequence(const std::vector<Gene>& sequence, std::map<int, std::vector<PlacedShape>>* out_placement = nullptr);
    
    // Создает соседнее состояние (мутация: обмен фигур местами или смена эвристики)
    State get_neighbor(const State& current, std::mt19937& rng);
};

// --- 4. Genetic Algorithm Solver (Coordinate-based) ---
class GeneticAlgorithmSolver : public Solver {
public:
    GeneticAlgorithmSolver(std::shared_ptr<Grid> g, const std::vector<Bundle>& b, SolverConfig cfg = SolverConfig()) 
        : Solver(g, b, cfg) {}
        
    float solve() override;

private:
    struct PlacedShape {
        int anchor_id;
        int rotation;
        std::shared_ptr<Figure> figure;
        std::vector<int> footprint;
    };

    struct Individual {
        // Генотип: bundle_id -> список размещенных фигур
        std::map<int, std::vector<PlacedShape>> active_bundles;
        // Фенотип: список занятых узлов (для быстрой проверки коллизий O(1))
        std::vector<char> occupied_nodes; 
        float fitness;
    };

    float calculate_fitness(const Individual& ind);
    Individual create_random_individual(std::mt19937& rng);
    Individual crossover(const Individual& p1, const Individual& p2, std::mt19937& rng);
    void mutate(Individual& ind, std::mt19937& rng);
    
    // Refactored helper methods
    bool try_add_bundle(Individual& ind, int bundle_id, std::mt19937& rng);
    std::vector<int> find_potential_anchors(const Individual& ind, const Bundle* bundle, std::mt19937& rng);
    bool try_place_shapes_at_anchor(Individual& ind, const Bundle* bundle, int anchor_candidate, std::mt19937& rng);
};

// --- 5. Genetic Permutation Solver ---
class GeneticPermutationSolver : public Solver {
public:
    GeneticPermutationSolver(std::shared_ptr<Grid> g, const std::vector<Bundle>& b, SolverConfig cfg = SolverConfig()) 
        : Solver(g, b, cfg) {}
        
    float solve() override;

private:
    struct Gene {
        int bundle_id;
        int heuristic;
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
