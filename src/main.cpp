#include <iostream>
#include <vector>
#include <map>
#include <memory>
#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>

#include "generators.h"
#include "solvers.h"
#include "utils/ConfigLoader.hpp"
#include "utils/Serializer.hpp"
#include "utils/Timer.hpp"

struct Args {
    std::string mode = "";
    std::string config = "";
    std::string input = "";
    std::string output = "";
    std::string algo = "grasp";
    double timeout = 0.0; // Таймаут в секундах
    bool verbose = false;
};

Args parse_args(int argc, char* argv[]) {
    Args args;
    for(int i=1; i<argc; ++i) {
        std::string arg = argv[i];
        if(arg == "--mode" && i+1 < argc) args.mode = argv[++i];
        else if(arg == "--config" && i+1 < argc) args.config = argv[++i];
        else if(arg == "--input" && i+1 < argc) args.input = argv[++i];
        else if(arg == "--output" && i+1 < argc) args.output = argv[++i];
        else if(arg == "--algo" && i+1 < argc) args.algo = argv[++i];
        else if((arg == "--timeout" || arg == "--time") && i+1 < argc) args.timeout = std::stod(argv[++i]);
        else if(arg == "--verbose" || arg == "-v") args.verbose = true;
    }
    return args;
}

int main(int argc, char* argv[]) {
    Args args = parse_args(argc, argv);

    if (args.mode.empty()) {
        if (argc >= 3) {
            args.mode = "generate";
            args.config = argv[1];
            args.output = argv[2];
        } else {
            std::cout << "Usage:\n"
                  << "  Generate: ./solver_cli --mode generate --config <cfg> --output <path>\n"
                  << "  Solve:    ./solver_cli --mode solve --input <json> --output <json> --algo <name> [--timeout <sec>]\n";
            return 1;
        }
    }

    if (args.mode == "generate") {
        if (args.config.empty() || args.output.empty()) {
            std::cerr << "Error: Missing --config or --output for generate mode." << std::endl;
            return 1;
        }
        GeneratorConfig config = ConfigLoader::load(args.config);
        PuzzleGenerator generator(config);
        
        // Генерируем полный пазл (решенный)
        Puzzle solved_puzzle = generator.generate();
        
        // 1. Сохраняем "Ответ" (Target) - идеальное разбиение до очистки
        std::string target_path = args.output;
        size_t lastindex = target_path.find_last_of("."); 
        if (lastindex != std::string::npos) {
            target_path = target_path.substr(0, lastindex) + "_target" + target_path.substr(lastindex); 
        } else {
            target_path += "_target.json";
        }
        Serializer::save(solved_puzzle, target_path);
        std::cout << "Generated target solution: " << target_path << std::endl;

        // 2. Создаем "Задачу" (очищаем сетку)
        Puzzle task_puzzle = solved_puzzle.clone();
        task_puzzle.clear_grid();
        
        Serializer::save(task_puzzle, args.output);
        std::cout << "Generated benchmark: " << args.output << std::endl;
    } 
    else if (args.mode == "solve") {
        if (args.input.empty() || args.output.empty()) {
             std::cerr << "Error: Missing --input or --output for solve mode." << std::endl;
             return 1;
        }
        
        Puzzle puzzle = Serializer::load(args.input);
        
        if (!puzzle.get_grid()) {
            std::cerr << "Failed to load input: " << args.input << std::endl;
            return 1;
        }
        
        // Создаем конфиг
        SolverConfig cfg;
        cfg.max_time_seconds = args.timeout;
        cfg.verbose = args.verbose;

        // Only GRASP is supported
        auto solver = std::make_unique<GRASPSolver>(puzzle, cfg);

        Timer timer;
        timer.start();
        auto result = solver->solve();
        float score = result.score;
        double duration = timer.get_elapsed_sec() * 1000.0;
        
        // Расчет статистики
        int total_cells = 0;
        float coverage = 0.0f;
        if (auto g = puzzle.get_grid()) {
            total_cells = g->size();
            if (total_cells > 0) coverage = (score / (float)total_cells) * 100.0f;
        }

        // Красивый вывод в консоль
        std::cout << "\n================ RESULT ================" << std::endl;
        std::cout << " Algorithm : " << args.algo << std::endl;
        std::cout << " Grid Size : " << puzzle.get_grid()->get_width() << "x" << puzzle.get_grid()->get_height() << std::endl;
        std::cout << " Duration  : " << duration << " ms" << std::endl;
        std::cout << " Score     : " << score << " / " << total_cells << std::endl;
        std::cout << " Coverage  : " << coverage << "%" << std::endl;
        std::cout << "========================================" << std::endl;
        
        // Сохраняем решенную сетку из солвера
        // Создаем новый Puzzle с решенной сеткой и исходными бандлами
        Puzzle solved_puzzle(solver->graph, puzzle.get_bundles(), "Solved");
        Serializer::save(solved_puzzle, args.output);
    } 
    else {
        std::cerr << "Unknown mode: " << args.mode << std::endl;
        return 1;
    }

    return 0;
}
