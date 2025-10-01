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

Draw::Draw(const std::vector<Team> &t, const std::vector<Game> &initialGames,
           const std::unordered_set<std::string> &bannedCountryMatchups,
           int pots, int teamsPerPot, int gamesPerTeam, int gamesPerPotPair,
           bool s)
    : numPots(pots), numTeamsPerPot(teamsPerPot), numGamesPerTeam(gamesPerTeam),
      numTeams(numPots * numTeamsPerPot), numGamesPerPotPair(gamesPerPotPair),
      suppress(s), teams(t), randomEngine(std::random_device{}()) {
    initializeState(initialGames, bannedCountryMatchups);
}

Draw::Draw(std::string teamsPath, std::string initialGamesPath,
           std::string bannedCountryMatchupsPath, int pots, int teamsPerPot,
           int gamesPerTeam, int gamesPerPotPair, bool s)
    : numPots(pots), numTeamsPerPot(teamsPerPot), numGamesPerTeam(gamesPerTeam),
      numTeams(numPots * numTeamsPerPot), numGamesPerPotPair(gamesPerPotPair),
      suppress(s), teams(readCSVTeams(teamsPath)),
      randomEngine(std::random_device{}()) {
    initializeState(initialGamesPath != ""
                        ? readTXTGames(initialGamesPath, teams)
                        : std::vector<Game>(),
                    bannedCountryMatchupsPath != ""
                        ? readTXTCountries(bannedCountryMatchupsPath)
                        : std::unordered_set<std::string>());
}

void Draw::initializeState(
    const std::vector<Game> &initialGames,
    const std::unordered_set<std::string> &bannedCountryMatchups) {
    for (size_t i = 0; i < teams.size(); i++) {
        numTeamsByCountry[teams[i].country] += 1;
        teamIndsByCountry[teams[i].country].push_back(i);
    }
    for (int potInd = 0; potInd < numPots; potInd++) {
        needsHomeAgainstPot.push_back(std::unordered_set<int>());
        needsAwayAgainstPot.push_back(std::unordered_set<int>());
        for (int teamInd = 0; teamInd < numTeams; teamInd++) {
            needsHomeAgainstPot[potInd].insert(teamInd);
            needsAwayAgainstPot[potInd].insert(teamInd);
            countryHomeNeeds[teams[teamInd].country + ":" +
                             std::to_string(potInd + 1)] += 1;
            countryAwayNeeds[teams[teamInd].country + ":" +
                             std::to_string(potInd + 1)] += 1;
        }
    }
    // create all possible matchups (home vs away status matters)
    // games must be contested btwn two teams of diff countries, and countries
    // cannot be banned from playing each other
    // max of numTeams * numGamesPerTeam
    for (int i = 0; i < numTeams - 1; i++) {
        for (int j = i + 1; j < numTeams; j++) {
            if (teams[i].country == teams[j].country ||
                bannedCountryMatchups.count(teams[i].country + ":" +
                                            teams[j].country) ||
                bannedCountryMatchups.count(teams[j].country + ":" +
                                            teams[i].country)) {
                continue;
            }
            allGames.push_back(Game(i, j));
            allGames.push_back(Game(j, i));
        }
    }
    // initialize draw state with initial games
    for (const Game &g : initialGames) {
        updateDrawState(g);
        if (!suppress) {
            std::cout << GRAY << teams[g.h].abbrev << "-" << teams[g.a].abbrev
                      << " " << teams[g.h].pot << "-" << teams[g.a].pot << RESET
                      << std::endl;
        }
        gamesByTeamInd[g.h].push_back(g);
        gamesByTeamInd[g.a].push_back(g);
        allGames.erase(std::remove_if(allGames.begin(), allGames.end(),
                                      [this](const Game &aG) {
                                          return !validRemainingGame(aG);
                                      }),
                       allGames.end());
    }
}

const std::vector<Game> Draw::getPickedGames() const {
    return pickedGames;
}

bool Draw::validRemainingGame(const Game &g) const {
    // g is Game under consideration; return true if Game is valid (should be
    // kept), false if invalid (should be removed)
    std::string homePot = std::to_string(teams[g.h].pot);
    std::string awayPot = std::to_string(teams[g.a].pot);
    if (pickedGamesTeamInds.count(std::to_string(g.h) + ":" +
                                  std::to_string(g.a)) || // Game already picked
        pickedGamesTeamInds.count(
            std::to_string(g.a) + ":" +
            std::to_string(g.h)) || // Game's reverse fixture already picked
        get_or(numHomeGamesByTeamInd, g.h, 0) ==
            numGamesPerTeam /
                2 || // Game's home team already has enough home games
        get_or(numAwayGamesByTeamInd, g.a, 0) ==
            numGamesPerTeam /
                2 || // Game's away team already has enough away games
        get_or(isPickedByTeamIndOppPotLocation,
               std::to_string(g.h) + ":" + awayPot + ":h",
               false) || // Game's home team has already played away
                         // team's pot (as home team)
        get_or(isPickedByTeamIndOppPotLocation,
               std::to_string(g.a) + ":" + homePot + ":a",
               false) || // Game's away team has already played home
                         // team's pot (as away team)
        get_or(numGamesByTeamIndOppCountry,
               std::to_string(g.h) + ":" + teams[g.a].country, 0) ==
            2 || // Game's home team has faced max opps from away team's country
        get_or(numGamesByTeamIndOppCountry,
               std::to_string(g.a) + ":" + teams[g.h].country, 0) ==
            2 // Game's away team has faced max opps from home team's country
    ) {
        return false;
    }
    return true;
}

bool Draw::dfsValidRemainingGame(const Game &g,
                                 const DFSContext &context) const {
    // g is Game under consideration; return true if Game is valid (should be
    // kept), false if invalid (should be removed)
    std::string homePot = std::to_string(teams[g.h].pot);
    std::string awayPot = std::to_string(teams[g.a].pot);
    if (teams[g.h].country ==
            teams[g.a].country || // home and away team from same country
        context.pickedGamesTeamInds.count(
            std::to_string(g.h) + ":" +
            std::to_string(g.a)) || // Game already picked
        context.pickedGamesTeamInds.count(
            std::to_string(g.a) + ":" +
            std::to_string(g.h)) || // Game's reverse fixture already picked
        get_or(context.numHomeGamesByTeamInd, g.h, 0) ==
            numGamesPerTeam /
                2 || // Game's home team already has enough home games
        get_or(context.numAwayGamesByTeamInd, g.a, 0) ==
            numGamesPerTeam /
                2 || // Game's away team already has enough away games
        get_or(context.isPickedByTeamIndOppPotLocation,
               std::to_string(g.h) + ":" + awayPot + ":h",
               false) || // Game's home team has already played
                         // away team's pot (as home team)
        get_or(context.isPickedByTeamIndOppPotLocation,
               std::to_string(g.a) + ":" + homePot + ":a",
               false) || // Game's away team has already played
                         // home team's pot (as away team)
        get_or(context.numGamesByTeamIndOppCountry,
               std::to_string(g.h) + ":" + teams[g.a].country, 0) ==
            2 || // Game's home team has faced max opps from away team's country
        get_or(context.numGamesByTeamIndOppCountry,
               std::to_string(g.a) + ":" + teams[g.h].country, 0) ==
            2 // Game's away team has faced max opps from home team's country
    ) {
        return false;
    }
    return true;
}

bool Draw::draw() {
    try {
        for (int pot = 1; pot <= numPots; pot++) {
            for (int i = 0; i < numTeamsPerPot; i++) {
                // choose a team in the current pot
                // retrieve all existing games involving this team
                // pick games until all team's games are picked
                // repeat
                int pickedTeamIndex = pickTeamIndex(pot);

                if (!suppress) {
                    Team pickedTeam = teams[pickedTeamIndex];
                    std::cout << POT_COLORS[pickedTeam.pot - 1] << "Pot "
                              << pickedTeam.pot << ": " << pickedTeam.abbrev
                              << RESET << std::endl;
                    std::vector<Game> teamGames =
                        gamesByTeamInd[pickedTeamIndex];
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

                while (gamesByTeamInd[pickedTeamIndex].size() <
                       static_cast<size_t>(numGamesPerTeam)) {
                    std::shuffle(allGames.begin(), allGames.end(),
                                 randomEngine);
                    Game g = pickGame();
                    updateDrawState(g);
                    // std::cout << GRAY << teams[g.h].abbrev << "-"
                    //           << teams[g.a].abbrev << RESET << std::endl;
                    gamesByTeamInd[g.h].push_back(g);
                    gamesByTeamInd[g.a].push_back(g);
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
            displayPots(true);
        }
        return false;
    }
}

bool Draw::draw(BS::light_thread_pool &pool) {
    try {
        while (pickedGames.size() <
               static_cast<size_t>(numGamesPerTeam * numTeams / 2)) {
            std::shuffle(allGames.begin(), allGames.end(), randomEngine);
            Game g = pickGame(pool);
            updateDrawState(g);
            gamesByTeamInd[g.h].push_back(g);
            gamesByTeamInd[g.a].push_back(g);
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
        numHomeGamesByTeamInd[g.h] -= 1;
        numAwayGamesByTeamInd[g.a] -= 1;
        numGamesByTeamIndOppCountry[std::to_string(g.h) + ":" +
                                    teams[g.a].country] -= 1;
        numGamesByTeamIndOppCountry[std::to_string(g.a) + ":" +
                                    teams[g.h].country] -= 1;
        isPickedByTeamIndOppPotLocation[std::to_string(g.h) + ":" + awayPot +
                                        ":h"] = false;
        isPickedByTeamIndOppPotLocation[std::to_string(g.a) + ":" + homePot +
                                        ":a"] = false;
        pickedGamesTeamInds.erase(std::to_string(g.h) + ":" +
                                  std::to_string(g.a));
        pickedGames.erase(
            std::remove(pickedGames.begin(), pickedGames.end(), g),
            pickedGames.end());
        needsHomeAgainstPot[teams[g.a].pot - 1].insert(g.h);
        needsAwayAgainstPot[teams[g.h].pot - 1].insert(g.a);
        countryHomeNeeds[teams[g.h].country + ":" + awayPot] += 1;
        countryAwayNeeds[teams[g.a].country + ":" + homePot] += 1;
    } else {
        numGamesByPotPair[homePot + ":" + awayPot] += 1;
        numHomeGamesByTeamInd[g.h] += 1;
        numAwayGamesByTeamInd[g.a] += 1;
        numGamesByTeamIndOppCountry[std::to_string(g.h) + ":" +
                                    teams[g.a].country] += 1;
        numGamesByTeamIndOppCountry[std::to_string(g.a) + ":" +
                                    teams[g.h].country] += 1;
        isPickedByTeamIndOppPotLocation[std::to_string(g.h) + ":" + awayPot +
                                        ":h"] = true;
        isPickedByTeamIndOppPotLocation[std::to_string(g.a) + ":" + homePot +
                                        ":a"] = true;
        pickedGamesTeamInds.insert(std::to_string(g.h) + ":" +
                                   std::to_string(g.a));
        pickedGames.push_back(g);
        needsHomeAgainstPot[teams[g.a].pot - 1].erase(g.h);
        needsAwayAgainstPot[teams[g.h].pot - 1].erase(g.a);
        countryHomeNeeds[teams[g.h].country + ":" + awayPot] -= 1;
        countryAwayNeeds[teams[g.a].country + ":" + homePot] -= 1;
    }
}

void Draw::dfsUpdateDrawState(const Game &g, DFSContext &context,
                              bool revert) const {
    std::string homePot = std::to_string(teams[g.h].pot);
    std::string awayPot = std::to_string(teams[g.a].pot);
    if (revert) {
        context.numGamesByPotPair[homePot + ":" + awayPot] -= 1;
        context.numHomeGamesByTeamInd[g.h] -= 1;
        context.numAwayGamesByTeamInd[g.a] -= 1;
        context.numGamesByTeamIndOppCountry[std::to_string(g.h) + ":" +
                                            teams[g.a].country] -= 1;
        context.numGamesByTeamIndOppCountry[std::to_string(g.a) + ":" +
                                            teams[g.h].country] -= 1;
        context.isPickedByTeamIndOppPotLocation[std::to_string(g.h) + ":" +
                                                awayPot + ":h"] = false;
        context.isPickedByTeamIndOppPotLocation[std::to_string(g.a) + ":" +
                                                homePot + ":a"] = false;
        context.pickedGamesTeamInds.erase(std::to_string(g.h) + ":" +
                                          std::to_string(g.a));
        context.pickedGames.erase(std::remove(context.pickedGames.begin(),
                                              context.pickedGames.end(), g),
                                  context.pickedGames.end());
        context.needsHomeAgainstPot[teams[g.a].pot - 1].insert(g.h);
        context.needsAwayAgainstPot[teams[g.h].pot - 1].insert(g.a);
        context.countryHomeNeeds[teams[g.h].country + ":" + awayPot] += 1;
        context.countryAwayNeeds[teams[g.a].country + ":" + homePot] += 1;
    } else {
        context.numGamesByPotPair[homePot + ":" + awayPot] += 1;
        context.numHomeGamesByTeamInd[g.h] += 1;
        context.numAwayGamesByTeamInd[g.a] += 1;
        context.numGamesByTeamIndOppCountry[std::to_string(g.h) + ":" +
                                            teams[g.a].country] += 1;
        context.numGamesByTeamIndOppCountry[std::to_string(g.a) + ":" +
                                            teams[g.h].country] += 1;
        context.isPickedByTeamIndOppPotLocation[std::to_string(g.h) + ":" +
                                                awayPot + ":h"] = true;
        context.isPickedByTeamIndOppPotLocation[std::to_string(g.a) + ":" +
                                                homePot + ":a"] = true;
        context.pickedGamesTeamInds.insert(std::to_string(g.h) + ":" +
                                           std::to_string(g.a));
        context.pickedGames.push_back(g);
        context.needsHomeAgainstPot[teams[g.a].pot - 1].erase(g.h);
        context.needsAwayAgainstPot[teams[g.h].pot - 1].erase(g.a);
        context.countryHomeNeeds[teams[g.h].country + ":" + awayPot] -= 1;
        context.countryAwayNeeds[teams[g.a].country + ":" + homePot] -= 1;
    }
}

void Draw::dfsSortRemainingGames(std::vector<Game> &remainingGames,
                                 const DFSContext &context,
                                 int sortMode) const {
    // stable sort remaining games based on sortMode

    // used within DFS to find solution faster, using multiple threads if
    // necessary

    // sortMode==0:
    // - sort by away team whose country has most teams, then by away team
    //   with most remaining unscheduled games
    // sortMode==1:
    // - sort by away team whose country has least teams, then by away team with
    //   most remaining unscheduled games
    // sortMode==2:
    // - sort by away team with most remaining unscheduled games
    std::stable_sort(
        remainingGames.begin(), remainingGames.end(),
        [this, sortMode, &context](const Game &g1, const Game &g2) {
            int countryTeams1 = numTeamsByCountry.at(teams[g1.a].country);
            int countryTeams2 = numTeamsByCountry.at(teams[g2.a].country);
            if (sortMode != 2 && countryTeams1 != countryTeams2) {
                if (sortMode == 0) {
                    return countryTeams1 > countryTeams2;
                } else {
                    return countryTeams1 < countryTeams2;
                }
            }
            int remainingGames1 =
                numGamesPerTeam -
                get_or(context.numHomeGamesByTeamInd, g1.a, 0) -
                get_or(context.numAwayGamesByTeamInd, g1.a, 0);
            int remainingGames2 =
                numGamesPerTeam -
                get_or(context.numHomeGamesByTeamInd, g2.a, 0) -
                get_or(context.numAwayGamesByTeamInd, g2.a, 0);
            return remainingGames1 > remainingGames2;
        });

    // (slower) alternative:
    // sortMode==0:
    // - sort by away team with least countryAwayNeeds, then by away team with
    //   least remaining unscheduled games
    // sortMode==1:
    // - sort by away team with most countryAwayNeeds, then by away team with
    //   least remaining unscheduled games
    // sortMode==2:
    // - sort by away team with least remaining unscheduled games
    // std::stable_sort(
    //     remainingGames.begin(), remainingGames.end(),
    //     [this, sortMode, &context](const Game &g1, const Game &g2) {
    //         int awayNeeds1 = context.countryAwayNeeds.at(
    //             teams[g1.a].country + ":" + std::to_string(teams[g1.h].pot));
    //         int awayNeeds2 = context.countryAwayNeeds.at(
    //             teams[g2.a].country + ":" + std::to_string(teams[g2.h].pot));
    //         if (sortMode != 2 && awayNeeds1 != awayNeeds2) {
    //             if (sortMode == 0) {
    //                 return awayNeeds1 < awayNeeds2;
    //             } else {
    //                 return awayNeeds1 > awayNeeds2;
    //             }
    //         }
    //         int remainingGames1 =
    //             numGamesPerTeam -
    //             get_or(context.numHomeGamesByTeam, g1.a, 0) -
    //             get_or(context.numAwayGamesByTeam, g1.a, 0);
    //         int remainingGames2 =
    //             numGamesPerTeam -
    //             get_or(context.numHomeGamesByTeam, g2.a, 0) -
    //             get_or(context.numAwayGamesByTeam, g2.a, 0);
    //         return remainingGames1 < remainingGames2;
    //     });
}

int Draw::pickTeamIndex(int pot) {
    // auto-pick random team index out of remaining teams in current pot
    std::vector<int> teamIndices(numTeamsPerPot);
    std::iota(teamIndices.begin(), teamIndices.end(),
              (pot - 1) * numTeamsPerPot);
    teamIndices.erase(std::remove_if(teamIndices.begin(), teamIndices.end(),
                                     [this](int teamIndex) {
                                         return drawnTeamInds.count(teamIndex);
                                     }),
                      teamIndices.end());
    std::shuffle(teamIndices.begin(), teamIndices.end(), randomEngine);
    int pickedTeamIndex = teamIndices[0];
    drawnTeamInds.insert(pickedTeamIndex);
    return pickedTeamIndex;
}

DFSContext Draw::createDFSContext() const {
    DFSContext currentDrawState;
    currentDrawState.pickedGames = pickedGames;
    currentDrawState.numGamesByPotPair = numGamesByPotPair;
    currentDrawState.numHomeGamesByTeamInd = numHomeGamesByTeamInd;
    currentDrawState.numAwayGamesByTeamInd = numAwayGamesByTeamInd;
    currentDrawState.numGamesByTeamIndOppCountry = numGamesByTeamIndOppCountry;
    currentDrawState.isPickedByTeamIndOppPotLocation =
        isPickedByTeamIndOppPotLocation;
    currentDrawState.pickedGamesTeamInds = pickedGamesTeamInds;
    currentDrawState.needsHomeAgainstPot = needsHomeAgainstPot;
    currentDrawState.needsAwayAgainstPot = needsAwayAgainstPot;
    currentDrawState.countryHomeNeeds = countryHomeNeeds;
    currentDrawState.countryAwayNeeds = countryAwayNeeds;
    return currentDrawState;
}

Game Draw::pickGame(BS::light_thread_pool &pool) const {
    // used in simulations to pick next game
    // use separate thread pools for outer simulations and inner DFS

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
        // perform "weak" checking first (each team needing away/home game
        // against g.h/g.a pot must have >= 1 valid matchup left), which should
        // be ok >80% of the time; if this leads to timeout, repeat with
        // "strong" checking (global country checks)
        try {
            if (testCandidateGame(g, pool, false)) {
                return g;
            }
        } catch (const TimeoutException &e) {
            if (testCandidateGame(g, pool, true)) {
                return g;
            }
        }
    }

    std::cerr << "Draw::pickGame() error: no game picked" << std::endl;
    exit(2);
}

bool Draw::testCandidateGame(const Game &g, BS::light_thread_pool &pool,
                             bool strongCheck) const {
    // g is candidate game
    // return true if valid game, false if invalid, throw TimeoutException if
    // timeout
    std::atomic<bool> stop{false};
    std::chrono::steady_clock::time_point deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(2500);
    std::promise<bool> resultPromise;
    std::shared_future<bool> resultFuture = resultPromise.get_future().share();

    // DFS with default sort order
    std::future<void> future =
        pool.submit_task([this, &g, &stop, &resultPromise, strongCheck]() {
            DFSContext currentDrawState = createDFSContext();
            bool result =
                dfs(g, allGames, currentDrawState, 0, strongCheck, stop);
            bool expected = false;
            if (stop.compare_exchange_strong(expected, true)) {
                resultPromise.set_value(result);
            }
        });

    // wait up to 250ms for default DFS
    if (resultFuture.wait_for(std::chrono::milliseconds(250)) ==
        std::future_status::ready) {
        // result returned by DFS within 250ms
        stop.store(true, std::memory_order_relaxed);
        future.wait();
        return resultFuture.get();
    }

    // default DFS hasn't finished, launch extra workers with different sort
    // orders
    std::vector<std::future<void>> futures;
    for (int sortMode = 1; sortMode < 3; sortMode++) {
        futures.push_back(pool.submit_task([this, sortMode, &g, &stop,
                                            &resultPromise, strongCheck]() {
            DFSContext currentDrawState = createDFSContext();
            bool result =
                dfs(g, allGames, currentDrawState, sortMode, strongCheck, stop);
            bool expected = false;
            if (stop.compare_exchange_strong(expected, true)) {
                resultPromise.set_value(result);
            }
        }));
    }

    // // wait until any thread finishes (no timeouts)
    // resultFuture.wait();
    // stop.store(true, std::memory_order_relaxed);
    // future.wait();
    // for (auto &f : futures) {
    //     f.wait();
    // }
    // return resultFuture.get();

    // wait until result or deadline
    if (resultFuture.wait_until(deadline) == std::future_status::ready) {
        // result returned before timeout
        stop.store(true, std::memory_order_relaxed);
        future.wait();
        for (auto &f : futures) {
            f.wait();
        }
        return resultFuture.get();
    } else {
        // timeout
        stop.store(true, std::memory_order_relaxed);
        future.wait();
        for (auto &f : futures) {
            f.wait();
        }
        throw TimeoutException();
    }
}

Game Draw::pickGame() const {
    // used in debug to pick next game (does not nec. involve picked team)
    // no thread pools: use raw threads for inner DFS

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
        // perform "weak" checking first (each team needing away/home game
        // against g.h/g.a pot must have >= 1 valid matchup left), which should
        // be ok >80% of the time; if this leads to timeout, repeat with
        // "strong" checking (global country checks)
        try {
            if (testCandidateGame(g, false)) {
                return g;
            }
        } catch (const TimeoutException &e) {
            if (testCandidateGame(g, true)) {
                return g;
            }
        }
    }

    std::cerr << "Draw::pickGame() error: no game picked" << std::endl;
    exit(2);
}

bool Draw::testCandidateGame(const Game &g, bool strongCheck) const {
    // g is candidate game
    // return true if valid game, false if invalid, throw TimeoutException if
    // timeout
    std::atomic<bool> stop{false};
    std::chrono::steady_clock::time_point deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(2500);
    std::promise<bool> resultPromise;
    std::shared_future<bool> resultFuture = resultPromise.get_future().share();

    // DFS with default sort order
    std::thread monitor([this, &g, &stop, &resultPromise, strongCheck]() {
        DFSContext currentDrawState = createDFSContext();
        bool result = dfs(g, allGames, currentDrawState, 0, strongCheck, stop);
        bool expected = false;
        if (stop.compare_exchange_strong(expected, true)) {
            resultPromise.set_value(result);
        }
    });

    // wait up to 250ms for default DFS
    if (resultFuture.wait_for(std::chrono::milliseconds(250)) ==
        std::future_status::ready) {
        // result returned by DFS within 250ms
        stop.store(true, std::memory_order_relaxed);
        monitor.join();
        return resultFuture.get();
    }

    // default DFS hasn't finished, launch extra workers with different sort
    // orders
    std::vector<std::thread> workers;
    for (int sortMode = 1; sortMode < 3; sortMode++) {
        workers.emplace_back([this, sortMode, &g, &stop, &resultPromise,
                              strongCheck]() {
            DFSContext currentDrawState = createDFSContext();
            bool result =
                dfs(g, allGames, currentDrawState, sortMode, strongCheck, stop);
            bool expected = false;
            if (stop.compare_exchange_strong(expected, true)) {
                resultPromise.set_value(result);
            }
        });
    }

    // // wait until any thread finishes (no timeouts)
    // resultFuture.wait();
    // stop.store(true, std::memory_order_relaxed);
    // monitor.join();
    // for (auto &t : workers) {
    //     t.join();
    // }
    // return resultFuture.get();

    // wait until result or deadline
    if (resultFuture.wait_until(deadline) == std::future_status::ready) {
        // result returned before timeout
        stop.store(true, std::memory_order_relaxed);
        monitor.join();
        for (auto &t : workers) {
            t.join();
        }
        return resultFuture.get();
    } else {
        // timeout
        stop.store(true, std::memory_order_relaxed);
        monitor.join();
        for (auto &t : workers) {
            t.join();
        }
        throw TimeoutException();
    }
}

bool Draw::dfs(const Game &g, const std::vector<Game> &remainingGames,
               DFSContext &context, int sortMode, bool strongCheck,
               std::atomic<bool> &stop) const {
    // g is candidate game
    // return true if timeout, another thread finished, or g accepted

    // timeout, or another thread finished:
    if (stop.load(std::memory_order_relaxed)) {
        return true;
    }

    // reject:
    if (!dfsValidRemainingGame(g, context)) {
        return false;
    }

    // tentatively pick g, then perform checks; if any check fails, revert state
    // and reject
    dfsUpdateDrawState(g, context);

    // accept:
    if (context.pickedGames.size() ==
        static_cast<size_t>(numTeams * numGamesPerTeam / 2)) {
        return true;
    }

    // weak checking (faster, but less pruning):
    if (!dfsWeakCheck(g, context)) {
        dfsUpdateDrawState(g, context, true);
        return false;
    }

    // strong checking (slower, but more pruning):
    if (strongCheck && !dfsStrongCheck(context)) {
        dfsUpdateDrawState(g, context, true);
        return false;
    }

    // recursive case: g picked
    // recurse, then revert state
    std::vector<Game> newRemainingGames;
    std::copy_if(remainingGames.begin(), remainingGames.end(),
                 std::back_inserter(newRemainingGames),
                 [this, &context](const Game &rG) {
                     return dfsValidRemainingGame(rG, context);
                 });

    // pick new home team by getting minimum pot pair with unallocated games and
    // taking incomplete home pot team whose country has most teams

    // get min pot pair
    int potPairHomePot = 0; // 1-indexed
    int potPairAwayPot = 0; // 1-indexed
    bool done = false;
    for (int i = 1; i <= numPots; i++) {
        for (int j = 1; j <= numPots; j++) {
            if (context.numGamesByPotPair[std::to_string(i) + ":" +
                                          std::to_string(j)] <
                numGamesPerPotPair) {
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
    // team with missing games against away pot (pot pairing for UECL)
    int newHomeTeamIndex = -1;
    std::vector<int> teamIndices(numTeamsPerPot);
    std::iota(teamIndices.begin(), teamIndices.end(),
              (potPairHomePot - 1) * numTeamsPerPot);
    std::sort(
        teamIndices.begin(), teamIndices.end(),
        [this, &context, potPairAwayPot](int team1, int team2) {
            //   return numTeamsByCountry[teams[team1].country] >
            //          numTeamsByCountry[teams[team2].country];
            return context.countryHomeNeeds[teams[team1].country + ":" +
                                            std::to_string(potPairAwayPot)] <
                   context.countryHomeNeeds[teams[team2].country + ":" +
                                            std::to_string(potPairAwayPot)];
        });
    for (int t : teamIndices) {
        if (dfsHomeTeamPredicate(t, potPairAwayPot, context)) {
            newHomeTeamIndex = t;
            break;
        }
    }

    // stable sort remaining games to improve performance
    std::vector<Game> candidateGames(newRemainingGames);
    dfsSortRemainingGames(candidateGames, context, sortMode);

    // filter candidates to only include games involving new home team and
    // matching away pot
    for (const Game &cG : candidateGames) {
        if (cG.h == newHomeTeamIndex && teams[cG.a].pot == potPairAwayPot) {
            if (dfs(cG, newRemainingGames, context, sortMode, strongCheck,
                    stop)) {
                // accept, timeout, or another thread finished
                // revert state and immediately return
                dfsUpdateDrawState(g, context, true);
                return true;
            }
        }
    }

    // no valid candidate game, so reject
    dfsUpdateDrawState(g, context, true);
    // std::cout << "\t\t\treject (exhausted candidates)" << std::endl;
    return false;
}

bool Draw::dfsWeakCheck(const Game &g, const DFSContext &context) const {
    // return true if checks passed; false if any check failed

    // - each team needing away game against g.h pot must have >= 1 valid
    //   matchup left; otherwise, return false
    for (int teamInd : context.needsAwayAgainstPot[teams[g.h].pot - 1]) {
        bool validMatchup = false;
        for (int i = 0; i < numTeamsPerPot; i++) {
            int homeTeamInd = (teams[g.h].pot - 1) * numTeamsPerPot + i;
            if (dfsValidRemainingGame(Game(homeTeamInd, teamInd), context)) {
                validMatchup = true;
                break;
            }
        }
        if (!validMatchup) {
            return false;
        }
    }

    // - each team needing home game against g.a pot must have >= 1 valid
    //   matchup left; otherwise, return false
    for (int teamInd : context.needsHomeAgainstPot[teams[g.a].pot - 1]) {
        bool validMatchup = false;
        for (int i = 0; i < numTeamsPerPot; i++) {
            int awayTeamInd = (teams[g.a].pot - 1) * numTeamsPerPot + i;
            if (dfsValidRemainingGame(Game(teamInd, awayTeamInd), context)) {
                validMatchup = true;
                break;
            }
        }
        if (!validMatchup) {
            return false;
        }
    }

    return true;
}

bool Draw::dfsStrongCheck(const DFSContext &context) const {
    // return true if checks passed; false if any check failed

    // - for each country and each pot, country's home games needed against the
    //   pot and country's away games needed against the pot must not exceed
    //   respective supply
    for (auto &p : teamIndsByCountry) {
        std::string country = p.first;
        std::vector<int> countryTeamInds = p.second;
        for (int pot = 1; pot <= numPots; pot++) {
            // remaining # of home games this country needs against this pot
            int homeDemand = context.countryHomeNeeds.at(country + ":" +
                                                         std::to_string(pot));

            // conservatively high est of avail slots this pot can provide
            // to this country for home games
            int homeSlots = 0;

            // remaining # of away games this country needs against this pot
            int awayDemand = context.countryAwayNeeds.at(country + ":" +
                                                         std::to_string(pot));

            // conservatively high est of avail slots this pot can provide
            // to this country for away games
            int awaySlots = 0;

            // compute total homeSlots and awaySlots this pot can provide
            for (int i = 0; i < numTeamsPerPot; i++) {
                int potTeamInd = numTeamsPerPot * (pot - 1) + i;

                // available home slots provided by this pot team
                int homeSlotsTeam = 0;

                // available away slots provided by this pot team
                int awaySlotsTeam = 0;

                // this pot team can contribute up to maxSlotsTeam to pot's
                // total home or away slots
                int maxSlotsTeam =
                    2 - get_or(context.numGamesByTeamIndOppCountry,
                               std::to_string(potTeamInd) + ":" + country, 0);

                // compute # of home slots and away slots this pot team can
                // provide
                for (const int &countryTeamInd : countryTeamInds) {
                    if (dfsValidRemainingGame(Game(countryTeamInd, potTeamInd),
                                              context)) {
                        homeSlotsTeam =
                            std::min(maxSlotsTeam, homeSlotsTeam + 1);
                    }
                    if (dfsValidRemainingGame(Game(potTeamInd, countryTeamInd),
                                              context)) {
                        awaySlotsTeam =
                            std::min(maxSlotsTeam, awaySlotsTeam + 1);
                    }
                }
                homeSlots += homeSlotsTeam;
                awaySlots += awaySlotsTeam;
            }
            if (homeDemand > homeSlots || awayDemand > awaySlots) {
                return false;
            }
        }
    }
    return true;
}

bool Draw::dfsHomeTeamPredicate(int homeTeamIndex, int awayPot,
                                const DFSContext &context) const {
    // return true to use new home team, false to reject
    return !get_or(context.isPickedByTeamIndOppPotLocation,
                   std::to_string(homeTeamIndex) + ":" +
                       std::to_string(awayPot) + ":h",
                   false);
}

void Draw::displayPots(bool showCountries) const {
    std::cout << "Games: " << pickedGames.size() << std::endl << std::endl;

    for (int i = 0; i < numPots; i++) {
        std::cout << POT_COLORS[i] << "Pot " << i + 1 << RESET << std::endl;
        for (int j = 0; j < numTeamsPerPot; j++) {
            int teamInd = i * numTeamsPerPot + j;
            std::cout << teams[teamInd].abbrev;
            if (showCountries) {
                std::cout << GRAY << "(" << toLower(teams[teamInd].country)
                          << ")" << RESET;
            }
            std::cout << "\t";
            std::vector<Game> teamGames = gamesByTeamInd.at(teamInd);
            std::stable_sort(teamGames.begin(), teamGames.end(),
                             [this, teamInd](const Game &g1, const Game &g2) {
                                 // order by opponent pot
                                 int oppInd1 = (g1.h == teamInd) ? g1.a : g1.h;
                                 int oppInd2 = (g2.h == teamInd) ? g2.a : g2.h;
                                 return teams[oppInd1].pot < teams[oppInd2].pot;
                             });
            for (size_t k = 0; k < teamGames.size(); k++) {
                Game g = teamGames[k];
                int oppInd = (g.h == teamInd) ? g.a : g.h;
                std::cout << POT_COLORS[teams[oppInd].pot - 1]
                          << teams[oppInd].abbrev << RESET;
                if (showCountries) {
                    std::cout << GRAY << "(" << toLower(teams[oppInd].country)
                              << "."
                              << numGamesByTeamIndOppCountry.at(
                                     std::to_string(teamInd) + ":" +
                                     teams[oppInd].country)
                              << ")" << RESET;
                }
                std::cout << ((g.h == teamInd) ? "h" : "a");
                if (k < teamGames.size() - 1) {
                    std::cout << ",";
                }
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;
    }
}

bool Draw::verifyDraw() const {
    // check for correct total number of games
    size_t numExpectedGames = numTeams * numGamesPerTeam / 2;
    if (pickedGames.size() != numExpectedGames) {
        if (!suppress)
            std::cout << "INVALID DRAW: drew " << pickedGames.size()
                      << " games but expected " << numExpectedGames << "."
                      << std::endl;
        return false;
    }

    std::unordered_map<int, TeamVerifier> m; // team ind -> TeamVerifier
    for (const Game &g : pickedGames) {
        // check for no opp from own country
        if (teams[g.h].country == teams[g.a].country) {
            if (!suppress)
                std::cout << "INVALID DRAW: same country ("
                          << teams[g.h].country << ")- " << teams[g.h].abbrev
                          << " has been drawn against " << teams[g.a].abbrev
                          << "." << std::endl;
            return false;
        }
        m[g.h].oppTeamInds.insert(g.a);
        m[g.h].numGamesByPot[teams[g.a].pot] += 1;
        m[g.a].oppTeamInds.insert(g.h);
        m[g.a].numGamesByPot[teams[g.h].pot] += 1;

        // check for no more than 2 opps from same country
        m[g.h].numOppsByCountry[teams[g.a].country] += 1;
        m[g.a].numOppsByCountry[teams[g.h].country] += 1;
        if (m[g.h].numOppsByCountry[teams[g.a].country] > 2 ||
            m[g.a].numOppsByCountry[teams[g.h].country] > 2) {
            if (!suppress)
                std::cout << "INVALID DRAW: > 2 country limit exceeded"
                          << std::endl;
            return false;
        }

        if (!verifyDrawHomeAway(m, g.h, g.a)) {
            return false;
        }
    }

    int oppsPerPot = numGamesPerTeam / numPots;
    for (auto &[teamInd, teamVerifierObj] : m) {
        // check for correct total num of opps
        if (teamVerifierObj.oppTeamInds.size() !=
            static_cast<size_t>(numGamesPerTeam)) {
            if (!suppress)
                std::cout << "INVALID DRAW: " << teams[teamInd].abbrev
                          << " has been drawn against "
                          << teamVerifierObj.oppTeamInds.size()
                          << " opponents (expected " << numGamesPerTeam << ")."
                          << std::endl;
            return false;
        }

        // check for correct num of opps per pot
        for (int i = 1; i <= numPots; i++) {
            if (teamVerifierObj.numGamesByPot[i] != oppsPerPot) {
                if (!suppress)
                    std::cout << "INVALID DRAW: " << teams[teamInd].abbrev
                              << " has been drawn against "
                              << teamVerifierObj.numGamesByPot[i]
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

bool Draw::verifyDrawHomeAway(std::unordered_map<int, TeamVerifier> &m,
                              int homeTeamIndex, int awayTeamIndex) const {
    // check for 1 home, 1 away game per pot for each team
    std::string hp = std::to_string(teams[homeTeamIndex].pot);
    std::string ap = std::to_string(teams[awayTeamIndex].pot);
    if (m[homeTeamIndex].isPickedByOppPotLocation[ap + ":h"] ||
        m[awayTeamIndex].isPickedByOppPotLocation[hp + ":a"]) {
        if (!suppress)
            std::cout << "INVALID DRAW: 1 home/1 away per pot violated"
                      << std::endl;
        return false;
    }
    m[homeTeamIndex].isPickedByOppPotLocation[ap + ":h"] = true;
    m[awayTeamIndex].isPickedByOppPotLocation[hp + ":a"] = true;
    return true;
}