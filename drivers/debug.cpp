#include "Draw.h"
#include "Simulator.h"
#include "globals.h"
#include "utils.h"
#include <iostream>
#include <vector>

// usage:
// $ make DRIVER=debug
// $ ./bin/debug <year> <ucl | uel | uecl>

int main(int argc, char **argv) {
    if (argc != 3) {
        std::cerr << "Usage: ./bin/debug <year> <competition>" << std::endl;
        exit(1);
    }

    const int year = std::stoi(argv[1]);
    const std::string competition = argv[2];

    if (year <= 0) {
        std::cerr << "Invalid year: must be > 0" << std::endl;
        exit(1);
    }

    if (competition != "ucl" && competition != "uel" && competition != "uecl") {
        std::cerr << "Invalid competition type: must be 'ucl', 'uel', or 'uecl'"
                  << std::endl;
        exit(1);
    }

    const std::string input_file =
        "data/" + std::to_string(year) + "/teams/" + competition + ".csv";

    Draw *d;
    if (competition == "ucl")
        d = new UCLDraw(input_file);
    else if (competition == "uel")
        d = new UELDraw(input_file);
    else if (competition == "uecl")
        d = new UECLDraw(input_file);

    bool valid = d->draw(false);
    if (valid) {
        d->displayPots();
        d->verifyDraw(false);
    }
}