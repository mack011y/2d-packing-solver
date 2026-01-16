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
    std::string stats = "";
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
        else if(arg == "--stats" && i+1 < argc) args.stats = argv[++i];
    }
    return args;
}

void write_stats(const std::string& filepath, const std::string& algo, int w, int h, double time_ms, float score, float max_score) {
    if (filepath.empty()) return;
    std::filesystem::path p(filepath);
    if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path());
    
    bool exists = std::filesystem::exists(filepath);
    std::ofstream f(filepath, std::ios::app);
    if (!exists) {
        f << "Algorithm,Width,Height,TimeMS,Score,MaxScore,CoveragePercent\n";
    }
    f << algo << "," << w << "," << h << "," 
      << time_ms << "," << score << "," << max_score << "," 
      << (score / max_score * 100.0f) << "\n";
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
                  << "  Solve:    ./solver_cli --mode solve --input <json> --output <json> --algo <name> [--stats <csv>]\n";
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
        std::shared_ptr<Grid> grid;
        auto result = generator.generate(grid);
        for(auto& node : grid->get_nodes()) {
            node.get_data().bundle_id = -1;
            node.get_data().figure_id = -1;
        }
        Serializer::save_json(args.output, grid, result.first);
        std::cout << "Generated benchmark: " << args.output << std::endl;
    } 
    else if (args.mode == "solve") {
        if (args.input.empty() || args.output.empty()) {
             std::cerr << "Error: Missing --input or --output for solve mode." << std::endl;
             return 1;
        }
        auto [grid, bundles] = Serializer::load_json(args.input);
        if (!grid) {
            std::cerr << "Failed to load input: " << args.input << std::endl;
            return 1;
        }
        std::unique_ptr<Solver> solver;
        if (args.algo == "dlx") solver = std::make_unique<DLXSolver>(grid, bundles);
        else if (args.algo == "grasp") solver = std::make_unique<GRASPSolver>(grid, bundles);
        else if (args.algo == "sa" || args.algo == "annealing") solver = std::make_unique<SimulatedAnnealingSolver>(grid, bundles);
        else if (args.algo == "ga" || args.algo == "genetic") solver = std::make_unique<GeneticAlgorithmSolver>(grid, bundles);
        else if (args.algo == "perm" || args.algo == "ga_perm") solver = std::make_unique<GeneticPermutationSolver>(grid, bundles);
        else {
            std::cerr << "Unknown algorithm: " << args.algo << ". Using GRASP as default." << std::endl;
            solver = std::make_unique<GRASPSolver>(grid, bundles);
        }
        
        Timer timer;
        timer.start();
        float score = solver->solve();
        double duration = timer.get_elapsed_sec() * 1000.0;
        
        std::cout << "Algorithm: " << args.algo << " | Time: " << duration << "ms | Score: " << score << std::endl;
        Serializer::save_json(args.output, grid, bundles);
        if (!args.stats.empty()) write_stats(args.stats, args.algo, grid->get_width(), grid->get_height(), duration, score, (float)grid->size());
    } 
    else {
        std::cerr << "Unknown mode: " << args.mode << std::endl;
        return 1;
    }

    return 0;
}
