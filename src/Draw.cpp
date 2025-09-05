#include "Draw.h"
#include "globals.h"
#include "utils.h"
#include <BS_thread_pool/BS_thread_pool.hpp>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <thread>
#include <vector>

const std::string POT_COLORS[] = {RED, BLUE, GREEN, YELLOW, CYAN, MAGENTA};

Draw::Draw(const std::vector<Team> &t, int pots, int teamsPerPot,
           int matchesPerTeam, int matchesPerPotPair)
    : numPots(pots), numTeamsPerPot(teamsPerPot),
      numMatchesPerTeam(matchesPerTeam), numTeams(numPots * numTeamsPerPot),
      numMatchesPerPotPair(matchesPerPotPair), teams(t),
      randomEngine(std::random_device{}()) {
    for (const Team &team : teams) {
        numTeamsByCountry[team.country] += 1;
    }
}

Draw::Draw(std::string input_path, int pots, int teamsPerPot,
           int matchesPerTeam, int matchesPerPotPair)
    : numPots(pots), numTeamsPerPot(teamsPerPot),
      numMatchesPerTeam(matchesPerTeam), numTeams(numPots * numTeamsPerPot),
      numMatchesPerPotPair(matchesPerPotPair), teams(readCSVTeams(input_path)),
      randomEngine(std::random_device{}()) {
    for (const Team &team : teams) {
        numTeamsByCountry[team.country] += 1;
    }
}

const std::vector<Game> &Draw::getSchedule() const {
    return pickedMatches;
}

void Draw::generateAllGames() {
    // create all possible matchups (home vs away status matters)
    // games must be contested btwn two teams of diff countries
    // max of numTeams * numMatchesPerTeam
    for (int i = 0; i < numTeams - 1; i++) {
        for (int j = i + 1; j < numTeams; j++) {
            if (teams[i].country != teams[j].country) {
                allGames.push_back(Game(i, j));
                allGames.push_back(Game(j, i));
            }
        }
    }
}

bool Draw::validRemainingGame(const Game &g) {
    // g is Game under consideration; return true if Game is valid (should be
    // kept), false if invalid (should be removed)
    std::string homePot = std::to_string(teams[g.h].pot);
    std::string awayPot = std::to_string(teams[g.a].pot);
    if (pickedMatchesTeamIndices.count(
            std::to_string(g.h) + ":" +
            std::to_string(g.a)) || // Game already picked
        pickedMatchesTeamIndices.count(
            std::to_string(g.a) + ":" +
            std::to_string(g.h)) || // Game's reverse fixture already picked
        numHomeGamesByTeam[g.h] ==
            numMatchesPerTeam /
                2 || // Game's home team already has enough home games
        numAwayGamesByTeam[g.a] ==
            numMatchesPerTeam /
                2 || // Game's away team already has enough away games
        hasPlayedWithPotMap[std::to_string(g.h) + ":" + awayPot +
                            ":h"] || // Game's home team has already played away
                                     // team's pot (as home team)
        hasPlayedWithPotMap[std::to_string(g.a) + ":" + homePot +
                            ":a"] || // Game's away team has already played home
                                     // team's pot (as away team)
        numOpponentCountryByTeam[std::to_string(g.h) + ":" +
                                 teams[g.a].country] ==
            2 || // Game's home team has faced max opps from away team's country
        numOpponentCountryByTeam[std::to_string(g.a) + ":" +
                                 teams[g.h].country] ==
            2 // Game's away team has faced max opps from home team's country
    ) {
        return false;
    }
    return true;
}

bool Draw::validRemainingGame(const Game &g, const DFSContext &context) {
    // g is Game under consideration; return true if Game is valid (should be
    // kept), false if invalid (should be removed)
    std::string homePot = std::to_string(teams[g.h].pot);
    std::string awayPot = std::to_string(teams[g.a].pot);
    if (context.pickedMatchesTeamIndices.count(
            std::to_string(g.h) + ":" +
            std::to_string(g.a)) || // Game already picked
        context.pickedMatchesTeamIndices.count(
            std::to_string(g.a) + ":" +
            std::to_string(g.h)) || // Game's reverse fixture already picked
        get_or(context.numHomeGamesByTeam, g.h, 0) ==
            numMatchesPerTeam /
                2 || // Game's home team already has enough home games
        get_or(context.numAwayGamesByTeam, g.a, 0) ==
            numMatchesPerTeam /
                2 || // Game's away team already has enough away games
        get_or(context.hasPlayedWithPotMap,
               std::to_string(g.h) + ":" + awayPot + ":h",
               false) || // Game's home team has already played
                         // away team's pot (as home team)
        get_or(context.hasPlayedWithPotMap,
               std::to_string(g.a) + ":" + homePot + ":a",
               false) || // Game's away team has already played
                         // home team's pot (as away team)
        get_or(context.numOpponentCountryByTeam,
               std::to_string(g.h) + ":" + teams[g.a].country, 0) ==
            2 || // Game's home team has faced max opps from away team's country
        get_or(context.numOpponentCountryByTeam,
               std::to_string(g.a) + ":" + teams[g.h].country, 0) ==
            2 // Game's away team has faced max opps from home team's country
    ) {
        return false;
    }
    return true;
}

bool Draw::draw(bool suppress) {
    try {
        generateAllGames();

        for (int pot = 1; pot <= numPots; pot++) {
            for (int i = 0; i < numTeamsPerPot; i++) {
                // choose a team in the current pot
                // retrieve all existing matches involving this team
                // pick matches until all team's matches are picked
                // repeat
                int pickedTeamIndex = pickTeamIndex(pot);

                if (!suppress) {
                    Team pickedTeam = teams[pickedTeamIndex];
                    std::cout << POT_COLORS[pickedTeam.pot - 1] << "Pot "
                              << pickedTeam.pot << ": " << pickedTeam.abbrev
                              << RESET << std::endl;
                    std::vector<Game> teamGames = gamesByTeam[pickedTeamIndex];
                    std::stable_sort(
                        teamGames.begin(), teamGames.end(),
                        [this, pickedTeamIndex](const Game &g1,
                                                const Game &g2) {
                            // order by opponent pot
                            int oppInd1 =
                                (g1.h == pickedTeamIndex) ? g1.a : g1.h;
                            int oppInd2 =
                                (g2.h == pickedTeamIndex) ? g2.a : g2.h;
                            return teams[oppInd1].pot < teams[oppInd2].pot;
                        });
                    for (const Game &g : teamGames) {
                        int oppInd = (g.h == pickedTeamIndex) ? g.a : g.h;
                        std::cout << teams[oppInd].pot << " "
                                  << POT_COLORS[teams[oppInd].pot - 1]
                                  << teams[oppInd].abbrev << RESET
                                  << ((g.h == pickedTeamIndex) ? "h" : "a")
                                  << std::endl;
                    }
                }

                while (gamesByTeam[pickedTeamIndex].size() <
                       static_cast<size_t>(numMatchesPerTeam)) {
                    std::shuffle(allGames.begin(), allGames.end(),
                                 randomEngine);
                    Game g = pickMatch();
                    updateDrawState(g);
                    gamesByTeam[g.h].push_back(g);
                    gamesByTeam[g.a].push_back(g);
                    allGames.erase(
                        std::remove_if(allGames.begin(), allGames.end(),
                                       [this](const Game &aG) {
                                           return !validRemainingGame(aG);
                                       }),
                        allGames.end());
                    if (!suppress) {
                        if (g.h == pickedTeamIndex || g.a == pickedTeamIndex) {
                            std::cout << teams[g.h].abbrev << "-"
                                      << teams[g.a].abbrev << "\t"
                                      << teams[g.h].pot << "-" << teams[g.a].pot
                                      << " " << allGames.size() << std::endl;
                        }
                    }
                }

                if (!suppress) {
                    std::cout << std::endl;
                }
            }
        }

        return true;
    } catch (const TimeoutException &e) {
        if (!suppress) {
            std::cout << RED << e.what() << RESET << std::endl;
        }
        return false;
    }
}

bool Draw::draw(BS::light_thread_pool &pool) {
    try {
        generateAllGames();
        while (pickedMatches.size() <
               static_cast<size_t>(numMatchesPerTeam * numTeams / 2)) {
            std::shuffle(allGames.begin(), allGames.end(), randomEngine);
            Game g = pickMatch(pool);
            updateDrawState(g);
            gamesByTeam[g.h].push_back(g);
            gamesByTeam[g.a].push_back(g);
            allGames.erase(std::remove_if(allGames.begin(), allGames.end(),
                                          [this](const Game &aG) {
                                              return !validRemainingGame(aG);
                                          }),
                           allGames.end());
        }
        return true;
    } catch (const TimeoutException &e) {
        return false;
    }
}

void Draw::updateDrawState(const Game &g, bool revert) {
    std::string homePot = std::to_string(teams[g.h].pot);
    std::string awayPot = std::to_string(teams[g.a].pot);
    if (revert) {
        numGamesByPotPair[homePot + ":" + awayPot] -= 1;
        numHomeGamesByTeam[g.h] -= 1;
        numAwayGamesByTeam[g.a] -= 1;
        numOpponentCountryByTeam[std::to_string(g.h) + ":" +
                                 teams[g.a].country] -= 1;
        numOpponentCountryByTeam[std::to_string(g.a) + ":" +
                                 teams[g.h].country] -= 1;
        hasPlayedWithPotMap[std::to_string(g.h) + ":" + awayPot + ":h"] = false;
        hasPlayedWithPotMap[std::to_string(g.a) + ":" + homePot + ":a"] = false;
        pickedMatchesTeamIndices.erase(std::to_string(g.h) + ":" +
                                       std::to_string(g.a));
        pickedMatches.erase(
            std::remove(pickedMatches.begin(), pickedMatches.end(), g),
            pickedMatches.end());
    } else {
        numGamesByPotPair[homePot + ":" + awayPot] += 1;
        numHomeGamesByTeam[g.h] += 1;
        numAwayGamesByTeam[g.a] += 1;
        numOpponentCountryByTeam[std::to_string(g.h) + ":" +
                                 teams[g.a].country] += 1;
        numOpponentCountryByTeam[std::to_string(g.a) + ":" +
                                 teams[g.h].country] += 1;
        hasPlayedWithPotMap[std::to_string(g.h) + ":" + awayPot + ":h"] = true;
        hasPlayedWithPotMap[std::to_string(g.a) + ":" + homePot + ":a"] = true;
        pickedMatchesTeamIndices.insert(std::to_string(g.h) + ":" +
                                        std::to_string(g.a));
        pickedMatches.push_back(g);
    }
}

void Draw::updateDrawState(const Game &g, DFSContext &context, bool revert) {
    std::string homePot = std::to_string(teams[g.h].pot);
    std::string awayPot = std::to_string(teams[g.a].pot);
    if (revert) {
        context.numGamesByPotPair[homePot + ":" + awayPot] -= 1;
        context.numHomeGamesByTeam[g.h] -= 1;
        context.numAwayGamesByTeam[g.a] -= 1;
        context.numOpponentCountryByTeam[std::to_string(g.h) + ":" +
                                         teams[g.a].country] -= 1;
        context.numOpponentCountryByTeam[std::to_string(g.a) + ":" +
                                         teams[g.h].country] -= 1;
        context
            .hasPlayedWithPotMap[std::to_string(g.h) + ":" + awayPot + ":h"] =
            false;
        context
            .hasPlayedWithPotMap[std::to_string(g.a) + ":" + homePot + ":a"] =
            false;
        context.pickedMatchesTeamIndices.erase(std::to_string(g.h) + ":" +
                                               std::to_string(g.a));
        context.pickedMatches.erase(std::remove(context.pickedMatches.begin(),
                                                context.pickedMatches.end(), g),
                                    context.pickedMatches.end());
    } else {
        context.numGamesByPotPair[homePot + ":" + awayPot] += 1;
        context.numHomeGamesByTeam[g.h] += 1;
        context.numAwayGamesByTeam[g.a] += 1;
        context.numOpponentCountryByTeam[std::to_string(g.h) + ":" +
                                         teams[g.a].country] += 1;
        context.numOpponentCountryByTeam[std::to_string(g.a) + ":" +
                                         teams[g.h].country] += 1;
        context
            .hasPlayedWithPotMap[std::to_string(g.h) + ":" + awayPot + ":h"] =
            true;
        context
            .hasPlayedWithPotMap[std::to_string(g.a) + ":" + homePot + ":a"] =
            true;
        context.pickedMatchesTeamIndices.insert(std::to_string(g.h) + ":" +
                                                std::to_string(g.a));
        context.pickedMatches.push_back(g);
    }
}

void Draw::sortRemainingGames(std::vector<Game> &remainingGames, int sortMode) {
    // stable sort remaining games based on sortMode

    // sortMode==0:
    // - sort by away team whose country has most teams, then by away team
    //   with most remaining matches
    // sortMode==1:
    // - sort by away team whose country has least teams, then by away team with
    //   most remaining matches
    // sortMode==2:
    // - sort by away team with most remaining matches
    std::stable_sort(remainingGames.begin(), remainingGames.end(),
                     [this, sortMode](const Game &g1, const Game &g2) {
                         int countryTeams1 =
                             numTeamsByCountry[teams[g1.a].country];
                         int countryTeams2 =
                             numTeamsByCountry[teams[g2.a].country];
                         if (sortMode != 2 && countryTeams1 != countryTeams2) {
                             if (sortMode == 0) {
                                 return countryTeams1 > countryTeams2;
                             } else {
                                 return countryTeams1 < countryTeams2;
                             }
                         }
                         int remainingMatches1 = numMatchesPerTeam -
                                                 numHomeGamesByTeam[g1.a] -
                                                 numAwayGamesByTeam[g1.a];
                         int remainingMatches2 = numMatchesPerTeam -
                                                 numHomeGamesByTeam[g2.a] -
                                                 numAwayGamesByTeam[g2.a];
                         return remainingMatches1 > remainingMatches2;
                     });
}

int Draw::pickTeamIndex(int pot) {
    // auto-pick random team index out of remaining teams in current pot
    std::vector<int> teamIndices(numTeamsPerPot);
    std::iota(teamIndices.begin(), teamIndices.end(),
              (pot - 1) * numTeamsPerPot);
    teamIndices.erase(std::remove_if(teamIndices.begin(), teamIndices.end(),
                                     [this](int teamIndex) {
                                         return drawnTeamIndices.count(
                                             teamIndex);
                                     }),
                      teamIndices.end());
    std::shuffle(teamIndices.begin(), teamIndices.end(), randomEngine);
    int pickedTeamIndex = teamIndices[0];
    drawnTeamIndices.insert(pickedTeamIndex);
    return pickedTeamIndex;
}

DFSContext Draw::createDFSContext() {
    DFSContext currentDrawState;
    currentDrawState.pickedMatches = pickedMatches;
    currentDrawState.numGamesByPotPair = numGamesByPotPair;
    currentDrawState.numHomeGamesByTeam = numHomeGamesByTeam;
    currentDrawState.numAwayGamesByTeam = numAwayGamesByTeam;
    currentDrawState.numOpponentCountryByTeam = numOpponentCountryByTeam;
    currentDrawState.hasPlayedWithPotMap = hasPlayedWithPotMap;
    currentDrawState.pickedMatchesTeamIndices = pickedMatchesTeamIndices;
    return currentDrawState;
}

Game Draw::pickMatch(BS::light_thread_pool &pool) {
    // use same thread pool for outer simulations and inner DFS

    // pick next match (does not necessarily involve picked team)
    std::vector<Game> orderedRemainingGames(allGames);

    // sort remaining games by home pot, then away pot
    std::stable_sort(orderedRemainingGames.begin(), orderedRemainingGames.end(),
                     [this](const Game &g1, const Game &g2) {
                         if (teams[g1.h].pot < teams[g2.h].pot)
                             return true;
                         if (teams[g2.h].pot < teams[g1.h].pot)
                             return false;
                         return teams[g1.a].pot < teams[g2.a].pot;
                     });

    for (const Game &g : orderedRemainingGames) {
        // within DFS, sort remaining games in different orders to find solution
        // faster, using multiple threads if necessary
        std::atomic<bool> stop{false};
        std::chrono::steady_clock::time_point deadline =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(2500);
        std::promise<bool> result_promise;
        std::shared_future<bool> result_future =
            result_promise.get_future().share();

        // DFS with default sort order
        std::future<void> future =
            pool.submit_task([this, &g, &stop, &result_promise]() {
                DFSContext currentDrawState = createDFSContext();
                bool result = DFS(g, allGames, currentDrawState, 0, stop);
                bool expected = false;
                if (stop.compare_exchange_strong(expected, true)) {
                    result_promise.set_value(result);
                }
            });

        // wait up to 250ms for default DFS
        if (result_future.wait_for(std::chrono::milliseconds(250)) ==
            std::future_status::ready) {
            // result returned by DFS within 250ms
            stop.store(true, std::memory_order_relaxed);
            future.wait();
            if (result_future.get()) {
                return g;
            } else {
                continue;
            }
        }

        // default DFS hasn't finished, launch extra workers
        std::vector<std::future<void>> futures;
        for (int sortMode = 1; sortMode < 3; sortMode++) {
            futures.push_back(pool.submit_task(
                [this, sortMode, &g, &stop, &result_promise]() {
                    DFSContext currentDrawState = createDFSContext();
                    bool result =
                        DFS(g, allGames, currentDrawState, sortMode, stop);
                    bool expected = false;
                    if (stop.compare_exchange_strong(expected, true)) {
                        result_promise.set_value(result);
                    }
                }));
        }

        // // wait until any thread finishes (no timeouts)
        // result_future.wait();
        // stop.store(true, std::memory_order_relaxed);
        // monitor.join();
        // for (auto &t : workers) {
        //     t.join();
        // }
        // if (result_future.get()) {
        //     return g;
        // } else {
        //     continue;
        // }

        // wait until result or deadline
        if (result_future.wait_until(deadline) == std::future_status::ready) {
            // result returned before timeout
            stop.store(true, std::memory_order_relaxed);
            future.wait();
            for (auto &f : futures) {
                f.wait();
            }
            if (result_future.get()) {
                return g;
            } else {
                continue;
            }
        } else {
            // timeout
            stop.store(true, std::memory_order_relaxed);
            future.wait();
            for (auto &f : futures) {
                f.wait();
            }
            throw TimeoutException();
            // alternatively, continue and only throw timeout exception if no
            // match picked - this could either lead to longer waits, or skip
            // slow DFS's.
        }
    }

    std::cerr << "Draw::pickMatch() error: no match picked" << std::endl;
    exit(2);
}

Game Draw::pickMatch() {
    // pick next match (does not necessarily involve picked team)
    std::vector<Game> orderedRemainingGames(allGames);

    // sort remaining games by home pot, then away pot
    std::stable_sort(orderedRemainingGames.begin(), orderedRemainingGames.end(),
                     [this](const Game &g1, const Game &g2) {
                         if (teams[g1.h].pot < teams[g2.h].pot)
                             return true;
                         if (teams[g2.h].pot < teams[g1.h].pot)
                             return false;
                         return teams[g1.a].pot < teams[g2.a].pot;
                     });

    for (const Game &g : orderedRemainingGames) {
        // within DFS, sort remaining games in different orders to find solution
        // faster, using multiple threads if necessary
        std::atomic<bool> stop{false};
        std::chrono::steady_clock::time_point deadline =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(2500);
        std::promise<bool> result_promise;
        std::shared_future<bool> result_future =
            result_promise.get_future().share();

        // options:
        // (1) use thread pool for outer simulations, raw threads for inner
        // (2) use same thread pool for outer and inner
        // (3) use separate thread pools for outer and inner (test different
        //     thread ratios)

        // DFS with default sort order
        std::thread monitor([this, &g, &stop, &result_promise]() {
            DFSContext currentDrawState = createDFSContext();
            bool result = DFS(g, allGames, currentDrawState, 0, stop);
            bool expected = false;
            if (stop.compare_exchange_strong(expected, true)) {
                result_promise.set_value(result);
            }
        });

        // wait up to 250ms for default DFS
        if (result_future.wait_for(std::chrono::milliseconds(250)) ==
            std::future_status::ready) {
            // result returned by DFS within 250ms
            stop.store(true, std::memory_order_relaxed);
            monitor.join();
            if (result_future.get()) {
                return g;
            } else {
                continue;
            }
        }

        // default DFS hasn't finished, launch extra workers
        std::vector<std::thread> workers;
        for (int sortMode = 1; sortMode < 3; sortMode++) {
            workers.emplace_back(
                [this, sortMode, &g, &stop, &result_promise]() {
                    DFSContext currentDrawState = createDFSContext();
                    bool result =
                        DFS(g, allGames, currentDrawState, sortMode, stop);
                    bool expected = false;
                    if (stop.compare_exchange_strong(expected, true)) {
                        result_promise.set_value(result);
                    }
                });
        }

        // // wait until any thread finishes (no timeouts)
        // result_future.wait();
        // stop.store(true, std::memory_order_relaxed);
        // monitor.join();
        // for (auto &t : workers) {
        //     t.join();
        // }
        // if (result_future.get()) {
        //     return g;
        // } else {
        //     continue;
        // }

        // wait until result or deadline
        if (result_future.wait_until(deadline) == std::future_status::ready) {
            // result returned before timeout
            stop.store(true, std::memory_order_relaxed);
            monitor.join();
            for (auto &t : workers) {
                t.join();
            }
            if (result_future.get()) {
                return g;
            } else {
                continue;
            }
        } else {
            // timeout
            stop.store(true, std::memory_order_relaxed);
            monitor.join();
            for (auto &t : workers) {
                t.join();
            }
            throw TimeoutException();
            // alternatively, continue and only throw timeout exception if no
            // match picked - this could either lead to longer waits, or skip
            // slow DFS's.
        }
    }

    std::cerr << "Draw::pickMatch() error: no match picked" << std::endl;
    exit(2);
}

bool Draw::DFS(const Game &g, const std::vector<Game> &remainingGames,
               DFSContext &context, int sortMode, std::atomic<bool> &stop) {
    // g is candidate game
    // return true if timeout, another thread finished, or g accepted

    // timeout, or another thread finished:
    if (stop.load(std::memory_order_relaxed)) {
        return true;
    }

    // reject:
    if (!validRemainingGame(g, context)) {
        return false;
    }

    // accept:
    if (context.pickedMatches.size() ==
        static_cast<size_t>((numTeams * numMatchesPerTeam / 2) - 1)) {
        return true;
    }

    // recursive case: g picked
    // update state, recurse, then revert state
    updateDrawState(g, context);
    std::vector<Game> newRemainingGames;
    std::copy_if(remainingGames.begin(), remainingGames.end(),
                 std::back_inserter(newRemainingGames),
                 [this, &context](const Game &rG) {
                     return validRemainingGame(rG, context);
                 });

    // pick new home team by getting minimum pot pair with unallocated matches
    // and taking incomplete home pot team whose country has most teams

    // get min pot pair
    int potPairHomePot = 0; // 1-indexed
    int potPairAwayPot = 0; // 1-indexed
    bool done = false;
    for (int i = 1; i <= numPots; i++) {
        for (int j = 1; j <= numPots; j++) {
            if (context.numGamesByPotPair[std::to_string(i) + ":" +
                                          std::to_string(j)] <
                numMatchesPerPotPair) {
                potPairHomePot = i;
                potPairAwayPot = j;
                done = true;
                break;
            }
        }
        if (done)
            break;
    }

    // sort home pot's team indices by country with most teams, then take first
    // team with missing matches against away pot (pot pairing for UECL)
    int newHomeTeamIndex = -1;
    std::vector<int> teamIndices(numTeamsPerPot);
    std::iota(teamIndices.begin(), teamIndices.end(),
              (potPairHomePot - 1) * numTeamsPerPot);
    std::sort(teamIndices.begin(), teamIndices.end(),
              [this](int team1, int team2) {
                  return numTeamsByCountry[teams[team1].country] >
                         numTeamsByCountry[teams[team2].country];
              });
    for (int t : teamIndices) {
        if (DFSHomeTeamPredicate(t, potPairAwayPot, context)) {
            newHomeTeamIndex = t;
            break;
        }
    }

    // stable sort remaining games to improve performance
    std::vector<Game> candidateGames(newRemainingGames);
    sortRemainingGames(candidateGames, sortMode);

    // filter candidates to only include games involving new home team and
    // matching away pot
    for (const Game &cG : candidateGames) {
        if (cG.h == newHomeTeamIndex && teams[cG.a].pot == potPairAwayPot) {
            if (DFS(cG, newRemainingGames, context, sortMode, stop)) {
                // accept, timeout, or another thread finished
                // revert state and immediately return
                updateDrawState(g, context, true);
                return true;
            }
        }
    }

    // no valid candidate game, so reject
    updateDrawState(g, context, true);
    return false;
}

int Draw::DFS(const Game &g, const std::vector<Game> &remainingGames,
              const std::chrono::steady_clock::time_point &t0, int sortMode) {
    // g is candidate game
    // return values: 0=reject, 1=accept, 2=timeout

    // handle timeout:
    auto t1 = std::chrono::steady_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);
    if (diff.count() > 5000) // timeout after 5s of DFS
    {
        // throw TimeoutException();
        return 2;
    }

    // reject:
    if (!validRemainingGame(g)) {
        return 0;
    }

    // accept:
    if (pickedMatches.size() ==
        static_cast<size_t>((numTeams * numMatchesPerTeam / 2) - 1)) {
        return 1;
    }

    // recursive case: g has been picked, update state, recurse, revert state
    updateDrawState(g);
    std::vector<Game> newRemainingGames;
    std::copy_if(remainingGames.begin(), remainingGames.end(),
                 std::back_inserter(newRemainingGames),
                 [this](const Game &rG) { return validRemainingGame(rG); });

    // pick new home team by getting minimum pot pair with unallocated matches
    // and taking incomplete home pot team whose country has most teams

    // get min pot pair
    int potPairHomePot = 0; // 1-indexed
    int potPairAwayPot = 0; // 1-indexed
    bool done = false;
    for (int i = 1; i <= numPots; i++) {
        for (int j = 1; j <= numPots; j++) {
            if (numGamesByPotPair[std::to_string(i) + ":" + std::to_string(j)] <
                numMatchesPerPotPair) {
                potPairHomePot = i;
                potPairAwayPot = j;
                done = true;
                break;
            }
        }
        if (done)
            break;
    }

    // sort home pot's team indices by country with most teams, then take first
    // team with missing matches against away pot (pot pairing for UECL)
    int newHomeTeamIndex = -1;
    std::vector<int> teamIndices(numTeamsPerPot);
    std::iota(teamIndices.begin(), teamIndices.end(),
              (potPairHomePot - 1) * numTeamsPerPot);
    std::sort(teamIndices.begin(), teamIndices.end(),
              [this](int team1, int team2) {
                  return numTeamsByCountry[teams[team1].country] >
                         numTeamsByCountry[teams[team2].country];
              });
    for (int t : teamIndices) {
        if (DFSHomeTeamPredicate(t, potPairAwayPot)) {
            newHomeTeamIndex = t;
            break;
        }
    }

    // stable sort remaining games to improve performance
    std::vector<Game> candidateGames(newRemainingGames);
    sortRemainingGames(candidateGames, sortMode);

    // filter candidates to only include games involving new home team and
    // matching away pot
    for (const Game &cG : candidateGames) {
        if (cG.h == newHomeTeamIndex && teams[cG.a].pot == potPairAwayPot) {
            int result = DFS(cG, newRemainingGames, t0, sortMode);
            if (result) {
                // accept or timeout, so revert state and immediately return
                updateDrawState(g, true);
                return result;
            }
        }
    }

    // no valid candidate game, so reject
    updateDrawState(g, true);
    return 0;
}

bool Draw::DFSHomeTeamPredicate(int homeTeamIndex, int awayPot) {
    // return true to use new home team, false to reject
    return !hasPlayedWithPotMap[std::to_string(homeTeamIndex) + ":" +
                                std::to_string(awayPot) + ":h"];
}

bool Draw::DFSHomeTeamPredicate(int homeTeamIndex, int awayPot,
                                const DFSContext &context) const {
    return !get_or(context.hasPlayedWithPotMap,
                   std::to_string(homeTeamIndex) + ":" +
                       std::to_string(awayPot) + ":h",
                   false);
}

void Draw::displayPots() const {
    std::cout << "Matches: " << pickedMatches.size() << std::endl << std::endl;

    for (int i = 0; i < numPots; i++) {
        std::cout << POT_COLORS[i] << "Pot " << i + 1 << RESET << std::endl;
        for (int j = 0; j < numTeamsPerPot; j++) {
            int teamInd = i * numTeamsPerPot + j;
            std::cout << teams[teamInd].abbrev << "\t";
            std::vector<Game> teamGames = gamesByTeam.at(teamInd);
            std::stable_sort(teamGames.begin(), teamGames.end(),
                             [this, teamInd](const Game &g1, const Game &g2) {
                                 // order by opponent pot
                                 int oppInd1 = (g1.h == teamInd) ? g1.a : g1.h;
                                 int oppInd2 = (g2.h == teamInd) ? g2.a : g2.h;
                                 return teams[oppInd1].pot < teams[oppInd2].pot;
                             });
            for (int k = 0; k < numMatchesPerTeam; k++) {
                Game g = teamGames[k];
                int oppInd = (g.h == teamInd) ? g.a : g.h;
                std::cout << POT_COLORS[teams[oppInd].pot - 1]
                          << teams[oppInd].abbrev << RESET
                          << ((g.h == teamInd) ? "h" : "a");
                if (k < numMatchesPerTeam - 1) {
                    std::cout << ",";
                }
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;
    }
}

bool Draw::verifyDraw(bool suppress) const {
    // check for correct total number of matches
    size_t expectedMatches = numTeams * numMatchesPerTeam / 2;
    if (pickedMatches.size() != expectedMatches) {
        if (!suppress)
            std::cout << "INVALID DRAW: drew " << pickedMatches.size()
                      << " matches but expected " << expectedMatches << "."
                      << std::endl;
        return false;
    }

    std::unordered_map<int, DrawVerifier> m; // team ind -> DrawVerifier
    for (const Game &g : pickedMatches) {
        // check for no opp from own country
        if (teams[g.h].country == teams[g.a].country) {
            if (!suppress)
                std::cout << "INVALID DRAW: same country ("
                          << teams[g.h].country << ")- " << teams[g.h].abbrev
                          << " has been drawn against " << teams[g.a].abbrev
                          << "." << std::endl;
            return false;
        }
        int homePot = teams[g.h].pot;
        int awayPot = teams[g.a].pot;
        m[g.h].opponents.insert(g.a);
        m[g.h].numMatchesPerPot[awayPot - 1] += 1;
        m[g.a].opponents.insert(g.h);
        m[g.a].numMatchesPerPot[homePot - 1] += 1;

        // check for no more than 2 opps from same country
        m[g.h].opponentCountryCount[teams[g.a].country] += 1;
        m[g.a].opponentCountryCount[teams[g.h].country] += 1;
        if (m[g.h].opponentCountryCount[teams[g.a].country] > 2 ||
            m[g.a].opponentCountryCount[teams[g.h].country] > 2) {
            if (!suppress)
                std::cout << "INVALID DRAW: > 2 country limit exceeded"
                          << std::endl;
            return false;
        }

        if (!verifyDrawHomeAway(m, g.h, g.a, suppress)) {
            return false;
        }
    }

    int oppsPerPot = numMatchesPerTeam / numPots;
    for (auto &[teamInd, teamVerifierObj] : m) {
        // check for correct total num of opps
        if (teamVerifierObj.opponents.size() !=
            static_cast<size_t>(numMatchesPerTeam)) {
            if (!suppress)
                std::cout << "INVALID DRAW: " << teams[teamInd].abbrev
                          << " has been drawn against "
                          << teamVerifierObj.opponents.size()
                          << " opponents (expected " << numMatchesPerTeam
                          << ")." << std::endl;
            return false;
        }

        // check for correct num of opps per pot
        for (int i = 0; i < numPots; i++) {
            if (teamVerifierObj.numMatchesPerPot[i] != oppsPerPot) {
                if (!suppress)
                    std::cout << "INVALID DRAW: " << teams[teamInd].abbrev
                              << " has been drawn against "
                              << teamVerifierObj.numMatchesPerPot[i]
                              << " clubs in Pot " << i + 1 << " (expected "
                              << oppsPerPot << ")." << std::endl;
                return false;
            }
        }
    }

    if (!suppress)
        std::cout << "Draw has been verified and is valid." << std::endl;
    return true;
}

bool Draw::verifyDrawHomeAway(std::unordered_map<int, DrawVerifier> &m,
                              int homeTeamIndex, int awayTeamIndex,
                              bool suppress) const {
    // check for 1 home, 1 away match per pot for each team
    std::string hp = std::to_string(teams[homeTeamIndex].pot);
    std::string ap = std::to_string(teams[awayTeamIndex].pot);
    if (m[homeTeamIndex].hasPlayedWithPot[ap + ":h"] ||
        m[awayTeamIndex].hasPlayedWithPot[hp + ":a"]) {
        if (!suppress)
            std::cout << "INVALID DRAW: 1 home/1 away per pot violated"
                      << std::endl;
        return false;
    }
    m[homeTeamIndex].hasPlayedWithPot[ap + ":h"] = true;
    m[awayTeamIndex].hasPlayedWithPot[hp + ":a"] = true;
    return true;
}