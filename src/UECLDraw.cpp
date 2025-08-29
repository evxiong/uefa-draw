#include "Draw.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

UECLDraw::UECLDraw() : Draw("data/teams/uecl.csv") {
    setParams(6, 6, 6);
}

UECLDraw::UECLDraw(std::string input_path) : Draw(input_path) {
    setParams(6, 6, 6);
}

UECLDraw::UECLDraw(const std::vector<Team> &t) : Draw(t) {
    setParams(6, 6, 6);
}

bool UECLDraw::verifyDraw(bool suppress) const {
    // check for correct total number of matches
    int expectedMatches = numTeams * numMatchesPerTeam / 2;
    if (pickedMatches.size() != expectedMatches) {
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

        // check for 1 home, 1 away match within paired pots (1/2, 3/4, 5/6) for
        // each team
        std::string hp = std::to_string(homePot);
        std::string pairedHp =
            std::to_string(homePot % 2 == 0 ? homePot - 1 : homePot + 1);
        std::string ap = std::to_string(awayPot);
        std::string pairedAp =
            std::to_string(awayPot % 2 == 0 ? awayPot - 1 : awayPot + 1);

        if (m[g.h].hasPlayedWithPot[ap + ":h"] ||
            m[g.h].hasPlayedWithPot[pairedAp + ":h"] ||
            m[g.a].hasPlayedWithPot[hp + ":a"] ||
            m[g.a].hasPlayedWithPot[pairedHp + ":a"]) {
            if (!suppress)
                std::cout
                    << "INVALID DRAW: 1 home/1 away per paired pots violated"
                    << std::endl;
            return false;
        }
        m[g.h].hasPlayedWithPot[ap + ":h"] = true;
        m[g.a].hasPlayedWithPot[hp + ":a"] = true;
    }
    for (auto &[teamInd, teamVerifierObj] : m) {
        // check for correct total num of opps
        if (teamVerifierObj.opponents.size() != numMatchesPerTeam) {
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
            if (teamVerifierObj.numMatchesPerPot[i] != 1) {
                if (!suppress)
                    std::cout << "INVALID DRAW: " << teams[teamInd].abbrev
                              << " has been drawn against "
                              << teamVerifierObj.numMatchesPerPot[i]
                              << " clubs in Pot " << i + 1 << " (expected 1)."
                              << std::endl;
                return false;
            }
        }
    }

    if (!suppress)
        std::cout << "Draw has been verified and is valid." << std::endl;
    return true;
}

bool UECLDraw::validRemainingGame(const Game &g, const Game &aG) {
    // g is picked Game, aG is unpicked Game under consideration
    // returns true if aG is valid (should be kept), false if invalid (should be
    // removed)
    int hp = teams[aG.h].pot;
    int ap = teams[aG.a].pot;
    std::string homePot = std::to_string(hp);
    std::string pairedHomePot = std::to_string(hp % 2 == 0 ? hp - 1 : hp + 1);
    std::string awayPot = std::to_string(ap);
    std::string pairedAwayPot = std::to_string(ap % 2 == 0 ? ap - 1 : ap + 1);
    if ((g.h == aG.h && g.a == aG.a) || // picked Game is now invalid
        (g.h == aG.a &&
         g.a == aG.h) || // picked Game's reverse fixture is now invalid
        numHomeGamesByTeam[aG.h] ==
            numMatchesPerTeam / 2 || // any Game whose home team has enough home
                                     // games is invalid
        numAwayGamesByTeam[aG.a] ==
            numMatchesPerTeam / 2 || // any Game whose away team has enough away
                                     // games is invalid
        hasPlayedWithPotMap[std::to_string(aG.h) + ":" + awayPot +
                            ":h"] || // any Game whose home team has already
                                     // played against away team's pot (either
                                     // home or away) is invalid
        hasPlayedWithPotMap[std::to_string(aG.h) + ":" + awayPot + ":a"] || //
        hasPlayedWithPotMap[std::to_string(aG.a) + ":" + homePot +
                            ":a"] || // any Game whose away team has already
                                     // played against home team's pot (either
                                     // home or away) is invalid
        hasPlayedWithPotMap[std::to_string(aG.a) + ":" + homePot + ":h"] || //
        hasPlayedWithPotMap[std::to_string(aG.h) + ":" + pairedAwayPot +
                            ":h"] || // any Game whose home team has already
                                     // played against away team's paired pot as
                                     // home team is invalid
        hasPlayedWithPotMap[std::to_string(aG.a) + ":" + pairedHomePot +
                            ":a"] // any Game whose away team has already played
                                  // against home team's paired pot as away team
                                  // is invalid
    ) {
        return false;
    }
    return true;
}

Game UECLDraw::pickMatch() {
    std::vector<Game> orderedRemainingGames(allGames);

    // order remaining games by min pot, then max pot
    std::stable_sort(orderedRemainingGames.begin(), orderedRemainingGames.end(),
                     [this](const Game &g1, const Game &g2) {
                         int g1min = std::min(teams[g1.h].pot, teams[g1.a].pot);
                         int g1max = std::max(teams[g1.h].pot, teams[g1.a].pot);
                         int g2min = std::min(teams[g2.h].pot, teams[g2.a].pot);
                         int g2max = std::max(teams[g2.h].pot, teams[g2.a].pot);
                         if (g1min == g1max && g2min != g2max)
                             return true;
                         if (g1min != g1max && g2min == g2max)
                             return false;
                         if (g1min == g2min)
                             return g1max < g2max;
                         return g1min < g2min;
                     });

    // for (const Game &g : orderedRemainingGames)
    // {
    //     std::cout << teams[g.h].pot << "-" << teams[g.a].pot << " ";
    // }
    // std::cout << std::endl;
    // exit(2);

    for (const Game &g : orderedRemainingGames) {
        bool result = DFS(g, allGames, 1, std::chrono::steady_clock::now());

        if (result) {
            return g;
        }
    }
    std::cout << "pickMatch error" << std::endl;
    exit(2);
}

bool UECLDraw::DFS(const Game &g, const std::vector<Game> &remainingGames,
                   int depth, const std::chrono::steady_clock::time_point &t0) {
    // g is candidate game

    // base case

    // reject:

    // handle timeout
    auto t1 = std::chrono::steady_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);
    if (diff.count() > 5000) // timeout after 5s of DFS
    {
        throw TimeoutException();
    }
    // return DFS(g, remainingGames, depth + 1, t0); // trigger timeout

    std::string homePot = std::to_string(teams[g.h].pot);
    std::string awayPot = std::to_string(teams[g.a].pot);

    // is this check even nec if we are already filtering remainingGames? maybe
    // add numOpponentCountryByTeam check to remainingGames predicate
    if (numHomeGamesByTeam[g.h] == numMatchesPerTeam / 2 ||
        numAwayGamesByTeam[g.a] == numMatchesPerTeam / 2 ||
        hasPlayedWithPotMap[std::to_string(g.h) + ":" + awayPot + ":h"] ||
        hasPlayedWithPotMap[std::to_string(g.a) + ":" + homePot + ":a"] ||
        numOpponentCountryByTeam[std::to_string(g.h) + ":" +
                                 teams[g.a].country] == 2 ||
        numOpponentCountryByTeam[std::to_string(g.a) + ":" +
                                 teams[g.h].country] == 2) {
        // std::cout << "Entered base case return false"
        return false;
    }

    // cerr << depth << "-  " << g.h << "-" << g.a << "  " <<
    // remainingGames.size() << std::endl;

    // accept:
    if (pickedMatches.size() == (numTeams * numMatchesPerTeam / 2) - 1) {
        return true;
    }

    // recursive case: g has been picked, update state, recurse, revert state
    updateDrawState(g);
    std::vector<Game> newRemainingGames;
    std::copy_if(
        remainingGames.begin(), remainingGames.end(),
        std::back_inserter(newRemainingGames),
        [&g, this](const Game &rG) { return validRemainingGame(g, rG); });

    // cerr << "-> " << newRemainingGames.size() << std::endl;

    // need to randomly choose home or away within paired pots
    // handle all matches + home/away alloc pot-by-pot over 6 choose 2 pots:
    // 1-1, 1-2, 1-3, 1-4, 1-5, 1-6, 2-2, 2-3, etc.

    // do within pot: 1-1, 2-2, 3-3, 4-4, 5-5, 6-6
    // then order by paired pots: 1-2/2-1, 1-3/3-1, 1-4/4-1
    int pot1, pot2; // 1-indexed
    bool done = false;
    for (int i = 1; i <= numPots; i++) {
        for (int j = i; j <= numPots; j++) {
            if ((i == j &&
                 numGamesByPotPair[std::to_string(i) + ":" +
                                   std::to_string(j)] < numTeamsPerPot / 2) ||
                (i != j && numGamesByPotPair[std::to_string(i) + ":" +
                                             std::to_string(j)] +
                                   numGamesByPotPair[std::to_string(j) + ":" +
                                                     std::to_string(i)] <
                               numTeamsPerPot)) {
                pot1 = i;
                pot2 = j;
                done = true;
                break;
            }
        }
        if (done)
            break;
    }

    // int newHome;
    // std::vector<int> v(numTeamsPerPot);
    // std::iota(v.begin(), v.end(), (potPairHomePot - 1) * numTeamsPerPot);
    // for (int t : v)
    // {
    //     if (!hasPlayedWithPotMap[std::to_string(t) + ":" +
    //     std::to_string(potPairAwayPot) + ":h"])
    //     {
    //         newHome = t;
    //         break;
    //     }
    // }

    for (const Game &rG : newRemainingGames) {
        // if (rG.h == newHome && teams[rG.a].pot == potPairAwayPot)
        if ((teams[rG.h].pot == pot1 && teams[rG.a].pot == pot2) ||
            (teams[rG.a].pot == pot1 && teams[rG.h].pot == pot2)) {
            bool result = DFS(rG, newRemainingGames, depth + 1, t0);
            if (result) {
                // revert state
                updateDrawState(g, true);
                return true;
            }
        }
    }

    updateDrawState(g, true);
    return false;
}
