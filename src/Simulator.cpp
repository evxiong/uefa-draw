#include "Simulator.h"
#include "Draw.h"
#include "globals.h"
#include "utils.h"
#include <BS_thread_pool/BS_thread_pool.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <indicators/indicators.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

Simulator::Simulator(int year, std::string c, std::string input,
                     std::string output)
    : competition(c) {
    input_file = (input == "") ? "data/" + std::to_string(year) + "/teams/" +
                                     competition + ".csv"
                               : input;
    output_path = std::filesystem::u8path(
        (output == "") ? "analysis/" + std::to_string(year) + "/sim/" +
                             competition + ".csv"
                       : output);

    // read input file csv to get teams
    teams = readCSVTeams(input_file);
}

void Simulator::run(int iterations) {
    indicators::show_console_cursor(false);
    indicators::BlockProgressBar bar{
        indicators::option::BarWidth{64},
        indicators::option::ForegroundColor{indicators::Color::white},
        indicators::option::FontStyles{
            std::vector<indicators::FontStyle>{indicators::FontStyle::bold}},
        indicators::option::MaxProgress{iterations},
        indicators::option::ShowElapsedTime{true},
        indicators::option::ShowRemainingTime{true},
    };

    std::cout << "Simulating " << iterations << " draws..." << std::endl;

    const int numThreads = std::thread::hardware_concurrency();
    BS::thread_pool pool(numThreads);

    // each thread keeps its own counts
    std::vector<std::unordered_map<std::string, int>> threadCounts(numThreads);
    std::vector<std::thread::id> threadIds = pool.get_thread_ids();
    std::unordered_map<std::thread::id, int> indexByThreadId;
    for (int i = 0; static_cast<size_t>(i) < threadIds.size(); i++) {
        indexByThreadId[threadIds[i]] = i;
    }

    // track progress
    std::atomic<int> failures{0};
    std::atomic<int> completed{0};
    std::atomic<int> duration{0}; // ms

    auto tStart = std::chrono::steady_clock::now();

    for (int i = 0; i < iterations; i++) {
        pool.detach_task([this, i, &pool, &indexByThreadId, &threadCounts,
                          &duration, &completed, &failures] {
            auto t0 = std::chrono::steady_clock::now();
            bool success = false;
            std::unique_ptr<Draw> d;
            while (!success) {
                if (competition == "ucl")
                    d.reset(new UCLDraw(teams));
                else if (competition == "uel")
                    d.reset(new UELDraw(teams));
                else if (competition == "uecl")
                    d.reset(new UECLDraw(teams));
                else {
                    std::cout << "Invalid competition specified" << std::endl;
                    exit(1);
                }
                d->draw();
                success = d->verifyDraw();
                if (!success) {
                    // update failures
                    failures.fetch_add(1, std::memory_order_relaxed);
                }
            }
            // update total duration
            auto t1 = std::chrono::steady_clock::now();
            auto diff =
                std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);
            duration.fetch_add(diff.count(), std::memory_order_relaxed);

            // update thread counts
            std::thread::id threadId = std::this_thread::get_id();
            std::vector<Game> pickedMatches = d->getSchedule();
            for (const Game &g : pickedMatches) {
                threadCounts[indexByThreadId.at(threadId)]
                            [std::to_string(g.h) + ":" + std::to_string(g.a)] +=
                    1;
            }

            // update completed count
            completed.fetch_add(1, std::memory_order_relaxed);
        });
    }

    // update progress bar
    while (completed.load() < iterations) {
        int i = completed.load();
        auto tCurrent = std::chrono::steady_clock::now();
        bar.set_option(indicators::option::PostfixText{
            std::to_string(i) + "/" + std::to_string(iterations) + " (" +
            std::to_string(duration.load() / static_cast<float>(1000 * i)) +
            "s / " +
            std::to_string(
                std::chrono::duration_cast<std::chrono::milliseconds>(tCurrent -
                                                                      tStart)
                    .count() /
                static_cast<float>(1000 * i)) +
            "s)"});
        bar.set_progress(i);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    pool.wait();

    // progress bar complete
    auto tCurrent = std::chrono::steady_clock::now();
    bar.set_option(indicators::option::PostfixText{
        std::to_string(iterations) + "/" + std::to_string(iterations) + " (" +
        std::to_string(duration.load() /
                       static_cast<float>(1000 * iterations)) +
        "s / " +
        std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                           tCurrent - tStart)
                           .count() /
                       static_cast<float>(1000 * iterations)) +
        "s)"});
    bar.set_progress(iterations);
    bar.mark_as_completed();
    indicators::show_console_cursor(true);

    // combine thread counts
    for (std::unordered_map<std::string, int> &local : threadCounts) {
        for (std::pair<const std::string, int> &kv : local) {
            counts[kv.first] += kv.second;
        }
    }

    writeResults();

    std::cout << "Failures: " << failures.load() << std::endl;
    std::cout << "Avg time per thread: "
              << duration.load() / static_cast<float>(1000 * iterations) << "s"
              << std::endl;
    std::cout << "Elapsed time per simulation: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(
                     tCurrent - tStart)
                         .count() /
                     static_cast<float>(1000 * iterations)
              << "s" << std::endl;
    std::cout << "Wrote results to " << output_path.string() << "."
              << std::endl;
}

void Simulator::writeResults() const {
    std::filesystem::create_directories(output_path.parent_path());
    std::ofstream out(output_path);
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