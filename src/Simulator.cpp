#include "Simulator.h"
#include "Draw.h"
#include "globals.h"
#include "utils.h"
#include <chrono>
#include <fstream>
#include <indicators/indicators.hpp>
#include <iostream>
#include <string>
#include <thread>

Simulator::Simulator(int y, std::string c, std::string input,
                     std::string output)
    : failures(0), year(y), competition(c) {
    input_file = (input == "") ? "data/" + std::to_string(year) + "/teams/" +
                                     competition + ".csv"
                               : input;
    output_file =
        (output == "") ? "analysis/sim/" + competition + ".csv" : output;

    // read input file csv to get teams
    teams = readCSVTeams(input_file);
}

void Simulator::run(int iterations) {
    indicators::show_console_cursor(false);
    using namespace indicators;
    BlockProgressBar bar{
        option::BarWidth{80}, option::ForegroundColor{Color::white},
        option::FontStyles{std::vector<FontStyle>{FontStyle::bold}},
        option::MaxProgress{iterations}};

    std::cout << "Simulating " << iterations << " draws..." << std::endl;

    float total = 0;
    int i = 0;

    while (i < iterations) {
        auto t0 = std::chrono::steady_clock::now();
        Draw *d;
        if (competition == "ucl")
            d = new UCLDraw(teams);
        else if (competition == "uel")
            d = new UELDraw(teams);
        else if (competition == "uecl")
            d = new UECLDraw(teams);
        d->draw();
        bool success = d->verifyDraw();

        auto t1 = std::chrono::steady_clock::now();
        auto diff =
            std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);
        total += float(diff.count()) / 1000;

        if (success) {
            updateCounts(d->getSchedule());
        }

        // Show iteration as postfix text
        bar.set_option(option::PostfixText{
            std::to_string(i + 1) + "/" + std::to_string(iterations) + " (" +
            std::to_string(total / (i + 1)) + "s" + ")"});

        if (success) {
            // update progress bar
            bar.tick();
            i += 1;
        } else {
            failures += 1;
        }
    }
    bar.mark_as_completed();

    // Show cursor
    indicators::show_console_cursor(true);

    writeResults();

    std::cout << "Failures: " << failures << std::endl;
    std::cout << "Avg time: " << total / iterations << "s" << std::endl;
    std::cout << "Wrote results to " << output_file << "." << std::endl;
}

void Simulator::updateCounts(const std::vector<Game> &s) {
    for (const Game &g : s) {
        counts[std::to_string(g.h) + ":" + std::to_string(g.a)] += 1;
    }
}

void Simulator::writeResults() const {
    std::ofstream out(output_file);
    out << "t1,t2,home,away,total\n";
    for (int i = 0; i < NUM_TEAMS - 1; i++) {
        for (int j = i + 1; j < NUM_TEAMS; j++) {
            std::string homeAwayKey =
                std::to_string(i) + ":" + std::to_string(j);
            std::string awayHomeKey =
                std::to_string(j) + ":" + std::to_string(i);
            int homeAwayCounts =
                counts.count(homeAwayKey) ? counts.at(homeAwayKey) : 0;
            int awayHomeCounts =
                counts.count(awayHomeKey) ? counts.at(awayHomeKey) : 0;
            out << i << "," << j << "," << homeAwayCounts << ","
                << awayHomeCounts << "," << homeAwayCounts + awayHomeCounts
                << std::endl;
        }
    }
}