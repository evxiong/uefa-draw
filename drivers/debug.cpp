// Run single simulation with full output displayed

#include "Draw.h"
#include "Simulator.h"
#include "globals.h"
#include "utils.h"
#include <iostream>
#include <memory>
#include <string>
#include <vector>

// usage:
// $ make DRIVER=debug
// $ ./bin/debug <year> <ucl | uel | uecl> <initial games txt path>

int main(int argc, char **argv) {
    if (argc < 3) {
        std::cerr << "Usage: ./bin/debug <year> <competition>" << std::endl;
        exit(1);
    }

    std::string initialGamesPath = "";
    const int year = std::stoi(argv[1]);
    const std::string competition = argv[2];

    if (argc >= 4) {
        initialGamesPath = argv[3];
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

    const std::string teamsPath =
        "data/" + std::to_string(year) + "/" + competition + "/teams.csv";

    const std::string bannedCountryMatchupsPath =
        "data/" + std::to_string(year) + "/banned.txt";

    std::unique_ptr<Draw> d;
    if (competition == "ucl")
        d.reset(new UCLDraw(teamsPath, initialGamesPath,
                            bannedCountryMatchupsPath, false));
    else if (competition == "uel")
        d.reset(new UELDraw(teamsPath, initialGamesPath,
                            bannedCountryMatchupsPath, false));
    else if (competition == "uecl")
        d.reset(new UECLDraw(teamsPath, initialGamesPath,
                             bannedCountryMatchupsPath, false));
    else {
        std::cerr << "Invalid competition type: must be 'ucl', 'uel', or 'uecl'"
                  << std::endl;
        exit(1);
    }

    bool valid = d->draw();
    if (valid) {
        d->displayPots();
        d->verifyDraw();
    }
}