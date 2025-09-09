#include "Draw.h"
#include "utils.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

UECLDraw::UECLDraw(std::string inputTeamsPath, std::string inputMatchesPath)
    : Draw(inputTeamsPath, inputMatchesPath, 6, 6, 6, 3) {}

UECLDraw::UECLDraw(const std::vector<Team> &t) : Draw(t, 6, 6, 6, 3) {}

bool UECLDraw::validRemainingGame(const Game &g) {
    if (!Draw::validRemainingGame(g)) {
        return false;
    }

    int hp = teams[g.h].pot;
    int ap = teams[g.a].pot;
    std::string homePot = std::to_string(hp);
    std::string pairedHomePot = std::to_string(hp % 2 == 0 ? hp - 1 : hp + 1);
    std::string awayPot = std::to_string(ap);
    std::string pairedAwayPot = std::to_string(ap % 2 == 0 ? ap - 1 : ap + 1);
    if (hasPlayedWithPotMap[std::to_string(g.h) + ":" + awayPot +
                            ":a"] || // Game's home team has already played away
                                     // team's pot (as away team)
        hasPlayedWithPotMap[std::to_string(g.a) + ":" + homePot +
                            ":h"] || // Game's away team has already played home
                                     // team's pot (as home team)
        hasPlayedWithPotMap[std::to_string(g.h) + ":" + pairedAwayPot +
                            ":h"] || // Game's home team has already played away
                                     // team's paired pot (as home team)
        hasPlayedWithPotMap[std::to_string(g.a) + ":" + pairedHomePot +
                            ":a"] // Game's away team has already played home
                                  // team's paired pot (as away team)
    ) {
        return false;
    }
    return true;
}

bool UECLDraw::validRemainingGame(const Game &g, const DFSContext &context) {
    if (!Draw::validRemainingGame(g, context)) {
        return false;
    }

    int hp = teams[g.h].pot;
    int ap = teams[g.a].pot;
    std::string homePot = std::to_string(hp);
    std::string pairedHomePot = std::to_string(hp % 2 == 0 ? hp - 1 : hp + 1);
    std::string awayPot = std::to_string(ap);
    std::string pairedAwayPot = std::to_string(ap % 2 == 0 ? ap - 1 : ap + 1);
    if (get_or(context.hasPlayedWithPotMap,
               std::to_string(g.h) + ":" + awayPot + ":a",
               false) || // Game's home team has already played away
                         // team's pot (as away team)
        get_or(context.hasPlayedWithPotMap,
               std::to_string(g.a) + ":" + homePot + ":h",
               false) || // Game's away team has already played home
                         // team's pot (as home team)
        get_or(context.hasPlayedWithPotMap,
               std::to_string(g.h) + ":" + pairedAwayPot + ":h",
               false) || // Game's home team has already played away
                         // team's paired pot (as home team)
        get_or(context.hasPlayedWithPotMap,
               std::to_string(g.a) + ":" + pairedHomePot + ":a",
               false) // Game's away team has already played home
                      // team's paired pot (as away team)
    ) {
        return false;
    }
    return true;
}

bool UECLDraw::DFSHomeTeamPredicate(int homeTeamIndex, int awayPot) {
    // return true to use new home team, false to reject
    std::string h = std::to_string(homeTeamIndex);
    std::string ap = std::to_string(awayPot);
    std::string pairedAp =
        std::to_string(awayPot % 2 == 0 ? awayPot - 1 : awayPot + 1);
    return !hasPlayedWithPotMap[h + ":" + ap + ":h"] &&
           !hasPlayedWithPotMap[h + ":" + ap + ":a"] &&
           !hasPlayedWithPotMap[h + ":" + pairedAp + ":h"];
}

bool UECLDraw::DFSHomeTeamPredicate(int homeTeamIndex, int awayPot,
                                    const DFSContext &context) const {
    std::string h = std::to_string(homeTeamIndex);
    std::string ap = std::to_string(awayPot);
    std::string pairedAp =
        std::to_string(awayPot % 2 == 0 ? awayPot - 1 : awayPot + 1);
    return !get_or(context.hasPlayedWithPotMap, h + ":" + ap + ":h", false) &&
           !get_or(context.hasPlayedWithPotMap, h + ":" + ap + ":a", false) &&
           !get_or(context.hasPlayedWithPotMap, h + ":" + pairedAp + ":h",
                   false);
}

bool UECLDraw::verifyDrawHomeAway(std::unordered_map<int, DrawVerifier> &m,
                                  int homeTeamIndex, int awayTeamIndex,
                                  bool suppress) const {
    // check for 1 home, 1 away match within paired pots (1/2, 3/4, 5/6) for
    // each team
    int homePot = teams[homeTeamIndex].pot;
    int awayPot = teams[awayTeamIndex].pot;

    std::string hp = std::to_string(homePot);
    std::string pairedHp =
        std::to_string(homePot % 2 == 0 ? homePot - 1 : homePot + 1);
    std::string ap = std::to_string(awayPot);
    std::string pairedAp =
        std::to_string(awayPot % 2 == 0 ? awayPot - 1 : awayPot + 1);

    if (m[homeTeamIndex].hasPlayedWithPot[ap + ":h"] ||
        m[homeTeamIndex].hasPlayedWithPot[pairedAp + ":h"] ||
        m[awayTeamIndex].hasPlayedWithPot[hp + ":a"] ||
        m[awayTeamIndex].hasPlayedWithPot[pairedHp + ":a"]) {
        if (!suppress)
            std::cout << "INVALID DRAW: 1 home/1 away per pot pairing violated"
                      << std::endl;
        return false;
    }
    m[homeTeamIndex].hasPlayedWithPot[ap + ":h"] = true;
    m[awayTeamIndex].hasPlayedWithPot[hp + ":a"] = true;
    return true;
}
