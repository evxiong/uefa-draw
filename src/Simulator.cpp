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

Simulator::Simulator(int y, std::string c, std::string teamsPath)
    : year(y), competition(c) {
    teams =
        readCSVTeams((teamsPath == "") ? "data/" + std::to_string(year) + "/" +
                                             competition + "/teams.csv"
                                       : teamsPath);
    bannedCountryMatchups =
        readTXTCountries("data/" + std::to_string(year) + "/banned.txt");
}

void Simulator::run(int iterations, std::string output) const {
    std::unordered_map<std::string, int> counts; // {homeInd}:{awayInd} -> count

    // compute results output path
    std::chrono::system_clock::time_point start =
        std::chrono::system_clock::now();
    std::filesystem::path outputPath = output;
    if (outputPath.empty()) {
        std::string timestamp = formatSystemTimePoint(start, "%Y%m%d_%H%M%S");
        std::string fileName = competition + "_" + std::to_string(year) + "_" +
                               std::to_string(iterations) + "_" + timestamp +
                               ".csv";
        // example path: `results/ucl_2025_1000_20250901_140522.csv`
        outputPath = "results/" + fileName;
    }

    // set up progress bar
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

    // set up thread pools
    const int numThreads = std::thread::hardware_concurrency();
    BS::thread_pool pool(numThreads);
    BS::thread_pool dfsPool(numThreads * 3);

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
        pool.detach_task([this, &dfsPool, &indexByThreadId, &threadCounts,
                          &duration, &completed, &failures] {
            auto t0 = std::chrono::steady_clock::now();
            bool success = false;
            std::unique_ptr<Draw> d;
            std::vector<Game> initialGames;
            bool hasFailed = false;

            while (!success) {
                if (competition == "ucl")
                    d.reset(new UCLDraw(teams, initialGames,
                                        bannedCountryMatchups));
                else if (competition == "uel")
                    d.reset(new UELDraw(teams, initialGames,
                                        bannedCountryMatchups));
                else if (competition == "uecl")
                    d.reset(new UECLDraw(teams, initialGames,
                                         bannedCountryMatchups));
                else {
                    std::cout << "Invalid competition specified" << std::endl;
                    exit(1);
                }
                d->draw(dfsPool);
                success = d->verifyDraw();
                if (!success) {
                    // if failed, replace initial games with current picked game
                    // state prior to failure
                    initialGames = d->getPickedGames();
                    hasFailed = true;
                }
            }

            // update failure count
            if (hasFailed) {
                failures.fetch_add(1, std::memory_order_relaxed);
            }

            // update thread counts
            std::thread::id threadId = std::this_thread::get_id();
            std::vector<Game> pickedGames = d->getPickedGames();
            for (const Game &g : pickedGames) {
                threadCounts[indexByThreadId.at(threadId)]
                            [std::to_string(g.h) + ":" + std::to_string(g.a)] +=
                    1;
            }

            // update completed count
            completed.fetch_add(1, std::memory_order_relaxed);

            // update total duration
            auto t1 = std::chrono::steady_clock::now();
            auto diff =
                std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);
            duration.fetch_add(diff.count(), std::memory_order_relaxed);
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

    writeResults(counts, outputPath, start, iterations);

    const int numFailures = failures.load();
    std::cout << "Failures: " << numFailures << std::endl;
    std::cout << "Avg time per thread: "
              << duration.load() / static_cast<float>(1000 * iterations) << "s"
              << std::endl;
    std::cout << "Elapsed time per simulation: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(
                     tCurrent - tStart)
                         .count() /
                     static_cast<float>(1000 * iterations)
              << "s" << std::endl;
    std::cout << "Wrote results to " << outputPath.string() << "." << std::endl;
}

void Simulator::writeResults(const std::unordered_map<std::string, int> &counts,
                             const std::filesystem::path &outputPath,
                             const std::chrono::system_clock::time_point &tp,
                             int iterations) const {
    std::filesystem::create_directories(outputPath.parent_path());
    std::ofstream out(outputPath);
    // write frontmatter
    out << "---\n";
    out << "timestamp: " << formatSystemTimePoint(tp, "%Y-%m-%d %H:%M:%S")
        << "\n";
    out << "competition: " << competition << "\n";
    out << "year: " << year << "\n";
    out << "simulations: " << iterations << "\n";
    out << "---\n";
    // write results
    out << "t1,t2,home,away,total\n";
    for (size_t i = 0; i < teams.size() - 1; i++) {
        for (size_t j = i + 1; j < teams.size(); j++) {
            std::string homeAwayKey =
                std::to_string(i) + ":" + std::to_string(j);
            std::string awayHomeKey =
                std::to_string(j) + ":" + std::to_string(i);
            int homeAwayCounts = get_or(counts, homeAwayKey, 0);
            int awayHomeCounts = get_or(counts, awayHomeKey, 0);
            out << i << "," << j << "," << homeAwayCounts << ","
                << awayHomeCounts << "," << homeAwayCounts + awayHomeCounts
                << std::endl;
        }
    }
}