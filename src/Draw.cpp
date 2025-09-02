#include "Draw.h"
#include "globals.h"
#include "utils.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <vector>

const std::string POT_COLORS[] = {RED, BLUE, GREEN, YELLOW, BLACK, MAGENTA};

Draw::Draw(const std::vector<Team> &t, int pots, int teamsPerPot,
           int matchesPerTeam)
    : numPots(pots), numTeamsPerPot(teamsPerPot),
      numMatchesPerTeam(matchesPerTeam), numTeams(numPots * numTeamsPerPot),
      teams(t), randomEngine(std::random_device{}()) {
    for (const Team &team : teams) {
        numTeamsByCountry[team.country] += 1;
    }
}

Draw::Draw(std::string input_path, int pots, int teamsPerPot,
           int matchesPerTeam)
    : numPots(pots), numTeamsPerPot(teamsPerPot),
      numMatchesPerTeam(matchesPerTeam), numTeams(numPots * numTeamsPerPot),
      teams(readCSVTeams(input_path)), randomEngine(std::random_device{}()) {
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

bool Draw::validRemainingGame(const Game &g, const Game &aG) {
    // g is picked Game, aG is unpicked Game under consideration
    // returns true if aG is valid (should be kept), false if invalid (should be
    // removed)
    std::string homePot = std::to_string(teams[aG.h].pot);
    std::string awayPot = std::to_string(teams[aG.a].pot);
    if ((g.h == aG.h && g.a == aG.a) || (g.h == aG.a && g.a == aG.h) ||
        numHomeGamesByTeam[aG.h] == numMatchesPerTeam / 2 ||
        numAwayGamesByTeam[aG.a] == numMatchesPerTeam / 2 ||
        hasPlayedWithPotMap[std::to_string(aG.h) + ":" + awayPot + ":h"] ||
        hasPlayedWithPotMap[std::to_string(aG.a) + ":" + homePot + ":a"]) {
        return false;
    }
    return true;
}

bool Draw::draw(bool suppress) {
    try {
        generateAllGames();

        size_t totalMatches = numTeams * numMatchesPerTeam / 2;

        for (int pot = 1; pot <= numPots; pot++) {
            for (int i = 0; i < numTeamsPerPot; i++) {
                // choose a team in the pot (either random, or user input)
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
                        [this](const Game &g1, const Game &g2) {
                            return std::max(teams[g1.h].pot, teams[g1.a].pot) <
                                   std::max(teams[g2.h].pot, teams[g2.a].pot);
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
                       numMatchesPerTeam) {
                    std::shuffle(allGames.begin(), allGames.end(),
                                 randomEngine);
                    // drawTeam(pot, suppress);
                    Game g = pickMatch();
                    updateDrawState(g);
                    gamesByTeam[g.h].push_back(g);
                    gamesByTeam[g.a].push_back(g);
                    allGames.erase(
                        std::remove_if(allGames.begin(), allGames.end(),
                                       [&g, this](const Game &aG) {
                                           return !validRemainingGame(g, aG);
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

            // // loop by pot
            // for (int pot = 1; pot <= numPots; pot++) {
            //     for (int i = 0; i < numTeamsPerPot; i++) {
            //         std::shuffle(allGames.begin(), allGames.end(),
            //                      randomEngine);
            //         // drawTeam(pot, suppress);
            //         Game g = pickMatch();
            //         updateDrawState(g);
            //         allGames.erase(
            //             std::remove_if(allGames.begin(), allGames.end(),
            //                            [&g, this](const Game &aG) {
            //                                return !validRemainingGame(g, aG);
            //                            }),
            //             allGames.end());
            //         if (!suppress) {
            //             std::cout << std::endl
            //                       << teams[g.h].abbrev << "-"
            //                       << teams[g.a].abbrev << "\t" <<
            //                       teams[g.h].pot
            //                       << "-" << teams[g.a].pot << " "
            //                       << allGames.size() << std::endl
            //                       << std::endl;
            //         }
            //     }
            // }

            // Game g = pickMatch();
            // updateDrawState(g);
            // allGames.erase(std::remove_if(allGames.begin(), allGames.end(),
            //                               [&g, this](const Game &aG) {
            //                                   return !validRemainingGame(g,
            //                                   aG);
            //                               }),
            //                allGames.end());
            // if (!suppress) {
            //     std::cout << teams[g.h].abbrev << "-" << teams[g.a].abbrev
            //               << "\t" << teams[g.h].pot << "-" << teams[g.a].pot
            //               << " " << allGames.size() << std::endl;
            // }
        }

        return true;
    } catch (const TimeoutException &e) {
        if (!suppress) {
            std::cout << RED << e.what() << RESET << std::endl;
        }
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

void Draw::sortRemainingGames(int sortMode, std::vector<Game> &remainingGames) {
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

void Draw::drawTeam(int pot, bool suppress) {
    // if auto-picking, pick a random team out of the remaining teams in the
    // current pot
    // if interactive, allow picking random or specific team from terminal

    // once team is picked, generate all remaining matches for this team.

    // (auto-pick) shuffle teams in current pot
    std::vector<int> teamIndices(numTeamsPerPot);
    std::iota(teamIndices.begin(), teamIndices.end(),
              (pot - 1) * numTeamsPerPot);
    teamIndices.erase(std::remove_if(teamIndices.begin(), teamIndices.end(),
                                     [this](int teamIndex) {
                                         return numHomeGamesByTeam[teamIndex] ==
                                                    numMatchesPerTeam / 2 ||
                                                numAwayGamesByTeam[teamIndex] ==
                                                    numMatchesPerTeam / 2;
                                     }),
                      teamIndices.end());
    std::shuffle(teamIndices.begin(), teamIndices.end(), randomEngine);

    if (teamIndices.size() == 0) {
        return;
    }

    int pickedTeamIndex = teamIndices[0];

    if (!suppress) {
        Team pickedTeam = teams[pickedTeamIndex];
        std::cout << POT_COLORS[pickedTeam.pot - 1] << "Pot " << pickedTeam.pot
                  << ": " << pickedTeam.abbrev << RESET << std::endl;
    }

    // filter remaining games to only include those involving picked team
    std::vector<Game> candidateGames;
    std::copy_if(allGames.begin(), allGames.end(),
                 std::back_inserter(candidateGames),
                 [pickedTeamIndex](const Game &g) {
                     return (g.h == pickedTeamIndex || g.a == pickedTeamIndex);
                 });

    // stable sort candidate games by max pot between teams
    // games within the same max pot between teams keep their shuffled order
    // ex. for a team in pot 1, games will be grouped: 1-1,1-2,1-3,1-4
    std::stable_sort(candidateGames.begin(), candidateGames.end(),
                     [this](const Game &g1, const Game &g2) {
                         return std::max(teams[g1.h].pot, teams[g1.a].pot) <
                                std::max(teams[g2.h].pot, teams[g2.a].pot);
                     });

    // allocate all games for this team
    // once a valid candidate game is found, update draw state
    // any future candidate games that conflict will be rejected on
    // first DFS call
    for (const Game &cG : candidateGames) {
        if (numHomeGamesByTeam[pickedTeamIndex] +
                numAwayGamesByTeam[pickedTeamIndex] ==
            numMatchesPerTeam) {
            break;
        }
        bool result = testCandidateGame(cG);
        if (result) {
            updateDrawState(cG);
            allGames.erase(std::remove_if(allGames.begin(), allGames.end(),
                                          [&cG, this](const Game &aG) {
                                              return !validRemainingGame(cG,
                                                                         aG);
                                          }),
                           allGames.end());
            if (!suppress) {
                std::cout << std::endl
                          << teams[cG.h].abbrev << "-" << teams[cG.a].abbrev
                          << "\t" << teams[cG.h].pot << "-" << teams[cG.a].pot
                          << " " << allGames.size() << std::endl
                          << std::endl;
            }
        }
    }
}

bool Draw::testCandidateGame(const Game &cG) {
    std::cerr << " -> cand " << teams[cG.h].abbrev << "-" << teams[cG.a].abbrev
              << " " << teams[cG.h].pot << "-" << teams[cG.a].pot;

    std::shuffle(allGames.begin(), allGames.end(), randomEngine);

    // stable sort remaining games to improve performance
    for (int sortMode = 0; sortMode < 3; sortMode++) {
        // std::vector<Game> remainingGames(allGames);
        // sortRemainingGames(sortMode, remainingGames);

        int result =
            DFS(cG, allGames, 1, std::chrono::steady_clock::now(), sortMode);
        if (result == 1) {
            // candidate game is valid
            return true;
        } else if (result == 0) {
            // candidate game rejected
            std::cerr << "\tr" << std::endl;
            return false;
        } else {
            // result == 2: timeout
            std::cerr << "\tTIMEOUT" << std::endl;
        }
    }
    throw TimeoutException();
}

Game Draw::pickMatch() {
    std::vector<Game> orderedRemainingGames(allGames);

    std::stable_sort(orderedRemainingGames.begin(), orderedRemainingGames.end(),
                     [this](const Game &g1, const Game &g2) {
                         if (teams[g1.h].pot < teams[g2.h].pot)
                             return true;
                         if (teams[g2.h].pot < teams[g1.h].pot)
                             return false;
                         return teams[g1.a].pot < teams[g2.a].pot;
                     });

    for (const Game &g : orderedRemainingGames) {
        // stable sort remaining games to improve performance
        for (int sortMode = 0; sortMode < 3; sortMode++) {
            int result =
                DFS(g, allGames, 1, std::chrono::steady_clock::now(), sortMode);
            if (result == 1) {
                return g;
            } else if (result == 0) {
                break;
            } else {
                if (sortMode == 2) {
                    throw TimeoutException();
                }
            }
        }
    }

    std::cout << "pickMatch error" << std::endl;
    exit(2);
}

int Draw::DFS(const Game &g, const std::vector<Game> &remainingGames, int depth,
              const std::chrono::steady_clock::time_point &t0, int sortMode) {
    // g is candidate game

    // base case

    // reject:

    // handle timeout
    auto t1 = std::chrono::steady_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);
    if (diff.count() > 5000) // timeout after 5s of DFS
    {
        // throw TimeoutException();
        return 2;
    }
    // return DFS(g, remainingGames, depth + 1, t0); // trigger timeout

    std::string homePot = std::to_string(teams[g.h].pot);
    std::string awayPot = std::to_string(teams[g.a].pot);

    if (pickedMatchesTeamIndices.count(std::to_string(g.h) + ":" +
                                       std::to_string(g.a)) ||
        pickedMatchesTeamIndices.count(std::to_string(g.a) + ":" +
                                       std::to_string(g.h)) ||
        numHomeGamesByTeam[g.h] == numMatchesPerTeam / 2 ||
        numAwayGamesByTeam[g.a] == numMatchesPerTeam / 2 ||
        hasPlayedWithPotMap[std::to_string(g.h) + ":" + awayPot + ":h"] ||
        hasPlayedWithPotMap[std::to_string(g.a) + ":" + homePot + ":a"] ||
        numOpponentCountryByTeam[std::to_string(g.h) + ":" +
                                 teams[g.a].country] == 2 ||
        numOpponentCountryByTeam[std::to_string(g.a) + ":" +
                                 teams[g.h].country] == 2) {
        return 0;
    }

    // cerr << depth << "-  " << g.h << "-" << g.a << "  " <<
    // remainingGames.size() << std::endl;

    // accept:
    if (pickedMatches.size() ==
        static_cast<size_t>((numTeams * numMatchesPerTeam / 2) - 1)) {
        return 1;
    }

    // recursive case: g has been picked, update state, recurse, revert state
    updateDrawState(g);
    std::vector<Game> newRemainingGames;
    std::copy_if(
        remainingGames.begin(), remainingGames.end(),
        std::back_inserter(newRemainingGames),
        [&g, this](const Game &rG) { return validRemainingGame(g, rG); });

    // cerr << "-> " << newRemainingGames.size() << std::endl;

    // pick new home team by taking incomplete team whose country has most teams
    // in current pot
    // get first incomplete pot pair
    int potPairHomePot, potPairAwayPot; // 1-indexed
    bool done = false;
    for (int i = 1; i <= numPots; i++) {
        for (int j = 1; j <= numPots; j++) {
            if (numGamesByPotPair[std::to_string(i) + ":" + std::to_string(j)] <
                numTeamsPerPot) {
                potPairHomePot = i;
                potPairAwayPot = j;
                done = true;
                break;
            }
        }
        if (done)
            break;
    }

    int newHome;
    std::vector<int> teamIndices(numTeamsPerPot);
    std::iota(teamIndices.begin(), teamIndices.end(),
              (potPairHomePot - 1) * numTeamsPerPot);

    // sort teamIndices by country with most teams first
    std::sort(teamIndices.begin(), teamIndices.end(),
              [this](int team1, int team2) {
                  return numTeamsByCountry[teams[team1].country] >
                         numTeamsByCountry[teams[team2].country];
              });
    for (int t : teamIndices) {
        if (!hasPlayedWithPotMap[std::to_string(t) + ":" +
                                 std::to_string(potPairAwayPot) + ":h"]) {
            newHome = t;
            break;
        }
    }

    std::vector<Game> candidateGames(newRemainingGames);
    sortRemainingGames(sortMode, candidateGames);

    // filter newRemainingGames to only include games with newHomeTeam and
    // matching away pot
    for (const Game &cG : candidateGames) {
        if (cG.h == newHome && teams[cG.a].pot == potPairAwayPot) {
            int result = DFS(cG, newRemainingGames, depth + 1, t0, sortMode);
            if (result) {
                // revert state
                updateDrawState(g, true);
                return result;
            }
        }
    }

    updateDrawState(g, true);
    return 0;
}

void Draw::displayPots() const {
    std::cout << "Matches: " << pickedMatches.size() << std::endl << std::endl;

    for (int i = 0; i < numPots; i++) {
        std::cout << POT_COLORS[i] << "Pot " << i + 1 << RESET << std::endl;
        for (int j = 0; j < numTeamsPerPot; j++) {
            int teamInd = i * numTeamsPerPot + j;
            std::cout << teams[teamInd].abbrev << "\t";
            std::vector<Game> teamGames = gamesByTeam.at(teamInd);
            std::stable_sort(
                teamGames.begin(), teamGames.end(),
                [this](const Game &g1, const Game &g2) {
                    return std::max(teams[g1.h].pot, teams[g1.a].pot) <
                           std::max(teams[g2.h].pot, teams[g2.a].pot);
                });
            for (int k = 0; k < numMatchesPerTeam; k++) {
                Game g = teamGames[k];
                int oppInd = (g.h == teamInd) ? g.a : g.h;
                std::cout << POT_COLORS[teams[oppInd].pot - 1]
                          << teams[oppInd].abbrev << RESET
                          << ((g.h == teamInd) ? "h" : "a") << ",";
                // std::cout << ((g.h == abbrev) ? BOLD : "") <<
                // POT_COLORS[DEFAULT_CLUBS.at(opponent).pot - 1] << opponent <<
                // RESET << ",";
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;
    }
}

bool Draw::verifyDraw(bool suppress) const {
    // check for correct total number of matches
    int expectedMatches = numTeams * numMatchesPerTeam / 2;
    if (pickedMatches.size() != static_cast<size_t>(expectedMatches)) {
        if (!suppress)
            std::cout << "INVALID DRAW: drew " << pickedMatches.size()
                      << " matches but expected " << expectedMatches << "."
                      << std::endl;
        return false;
    }

    std::unordered_map<int, DrawVerifier> m; // abbrev -> ClubVerifier
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

        // check for 1 home, 1 away match per pot for each team
        std::string hp = std::to_string(homePot);
        std::string ap = std::to_string(awayPot);
        if (m[g.h].hasPlayedWithPot[ap + ":h"] ||
            m[g.a].hasPlayedWithPot[hp + ":a"]) {
            if (!suppress)
                std::cout << "INVALID DRAW: 1 home/1 away per pot violated"
                          << std::endl;
            return false;
        }
        m[g.h].hasPlayedWithPot[ap + ":h"] = true;
        m[g.a].hasPlayedWithPot[hp + ":a"] = true;
    }
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
            if (teamVerifierObj.numMatchesPerPot[i] != 2) {
                if (!suppress)
                    std::cout << "INVALID DRAW: " << teams[teamInd].abbrev
                              << " has been drawn against "
                              << teamVerifierObj.numMatchesPerPot[i]
                              << " clubs in Pot " << i + 1 << " (expected 2)."
                              << std::endl;
                return false;
            }
        }
    }

    if (!suppress)
        std::cout << "Draw has been verified and is valid." << std::endl;
    return true;
}