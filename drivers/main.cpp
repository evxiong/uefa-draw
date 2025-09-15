// Simulate draws

#include "Draw.h"
#include "Simulator.h"
#include <iostream>
#include <string>

// usage:
// $ make all
// $ ./bin/main <year> <ucl | uel | uecl> <iterations> [<teams csv path>
//   <output csv path>]

int main(int argc, char **argv) {
    if (argc > 5) {
        std::cerr << "Too many arguments" << std::endl;
        exit(1);
    } else if (argc < 3) {
        std::cerr << "Missing arguments" << std::endl;
        exit(1);
    }

    int iterations = 1;
    std::string teamsPath = "";
    std::string output = "";
    int year = std::stoi(argv[1]);
    std::string competition = argv[2];

    if (argc >= 4) {
        iterations = std::stoi(argv[3]);
    }
    if (argc >= 5) {
        teamsPath = argv[4];
    }
    if (argc >= 6) {
        output = argv[5];
    }

    if (year <= 0) {
        std::cerr << "Invalid year: must be > 0" << std::endl;
        exit(1);
    }

    if (competition != "ucl" && competition != "uel" && competition != "uecl") {
        std::cerr << "Invalid competition type: must be 'ucl', 'uel', or 'uecl'"
                  << std::endl;
        exit(1);
    }

    Simulator s(year, competition, teamsPath);
    s.run(iterations, output);
    return 0;
}
