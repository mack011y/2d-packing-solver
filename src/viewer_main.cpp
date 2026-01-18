#include "Viewer.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input_json>" << std::endl;
        return 1;
    }
    
    Viewer viewer;
    return viewer.run(argv[1]);
}
