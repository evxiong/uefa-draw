#include "Draw.h"
#include "utils.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

UECLDraw::UECLDraw(std::string inputTeamsPath, std::string inputMatchesPath,
                   bool suppress)
    : Draw(inputTeamsPath, inputMatchesPath, 6, 6, 6, 3, suppress) {}

UECLDraw::UECLDraw(const std::vector<Team> &t, const std::vector<Game> &m,
                   bool suppress)
    : Draw(t, m, 6, 6, 6, 3, suppress) {}

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

void UECLDraw::updateDrawState(const Game &g, bool revert) {
    Draw::updateDrawState(g, revert);

    int hp = teams[g.h].pot;
    int ap = teams[g.a].pot;
    int pairedHomePot = hp % 2 == 0 ? hp - 1 : hp + 1;
    int pairedAwayPot = ap % 2 == 0 ? ap - 1 : ap + 1;
    if (revert) {
        needsHomeAgainstPot[pairedAwayPot - 1].insert(g.h);
        needsAwayAgainstPot[pairedHomePot - 1].insert(g.a);
        countryHomeNeeds[teams[g.h].country + ":" +
                         std::to_string(pairedAwayPot)] += 1;
        countryAwayNeeds[teams[g.a].country + ":" +
                         std::to_string(pairedHomePot)] += 1;
    } else {
        needsHomeAgainstPot[pairedAwayPot - 1].erase(g.h);
        needsAwayAgainstPot[pairedHomePot - 1].erase(g.a);
        countryHomeNeeds[teams[g.h].country + ":" +
                         std::to_string(pairedAwayPot)] -= 1;
        countryAwayNeeds[teams[g.a].country + ":" +
                         std::to_string(pairedHomePot)] -= 1;
    }
}

void UECLDraw::updateDrawState(const Game &g, DFSContext &context,
                               bool revert) {
    Draw::updateDrawState(g, context, revert);

    int hp = teams[g.h].pot;
    int ap = teams[g.a].pot;
    int pairedHomePot = hp % 2 == 0 ? hp - 1 : hp + 1;
    int pairedAwayPot = ap % 2 == 0 ? ap - 1 : ap + 1;
    if (revert) {
        context.needsHomeAgainstPot[pairedAwayPot - 1].insert(g.h);
        context.needsAwayAgainstPot[pairedHomePot - 1].insert(g.a);
        context.countryHomeNeeds[teams[g.h].country + ":" +
                                 std::to_string(pairedAwayPot)] += 1;
        context.countryAwayNeeds[teams[g.a].country + ":" +
                                 std::to_string(pairedHomePot)] += 1;
    } else {
        context.needsHomeAgainstPot[pairedAwayPot - 1].erase(g.h);
        context.needsAwayAgainstPot[pairedHomePot - 1].erase(g.a);
        context.countryHomeNeeds[teams[g.h].country + ":" +
                                 std::to_string(pairedAwayPot)] -= 1;
        context.countryAwayNeeds[teams[g.a].country + ":" +
                                 std::to_string(pairedHomePot)] -= 1;
    }
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
                                  int homeTeamIndex, int awayTeamIndex) const {
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

bool UECLDraw::DFSWeakCheck(const Game &g, const DFSContext &context) {
    // return true if checks passed; false if any check failed

    // - each team needing away game against g.h pot or g.h paired pot must have
    //   >= 1 valid matchup left; otherwise, return false
    // - needsAwayAgainstPot teams are identical for paired pots
    int homePot = teams[g.h].pot;
    for (int teamInd : context.needsAwayAgainstPot[homePot - 1]) {
        bool validMatchup = false;
        // check g.h pot and g.h paired pot
        for (int i = 0; i < numTeamsPerPot; i++) {
            int homeTeamInd = (homePot - 1) * numTeamsPerPot + i;
            int pairedPot = homePot % 2 == 0 ? homePot - 1 : homePot + 1;
            int pairedPotHomeTeamInd = (pairedPot - 1) * numTeamsPerPot + i;
            if (validRemainingGame(Game(homeTeamInd, teamInd), context) ||
                validRemainingGame(Game(pairedPotHomeTeamInd, teamInd),
                                   context)) {
                validMatchup = true;
                break;
            }
        }
        if (!validMatchup) {
            return false;
        }
    }

    // - each team needing home game against g.a pot or g.a paired pot must have
    //   >= 1 valid matchup left; otherwise, return false
    // - needsHomeAgainstPot teams are identical for paired pots
    int awayPot = teams[g.a].pot;
    for (int teamInd : context.needsHomeAgainstPot[awayPot - 1]) {
        bool validMatchup = false;
        // check g.a pot and g.a paired pot
        for (int i = 0; i < numTeamsPerPot; i++) {
            int awayTeamInd = (awayPot - 1) * numTeamsPerPot + i;
            int pairedPot = awayPot % 2 == 0 ? awayPot - 1 : awayPot + 1;
            int pairedPotAwayTeamInd = (pairedPot - 1) * numTeamsPerPot + i;
            if (validRemainingGame(Game(teamInd, awayTeamInd), context) ||
                validRemainingGame(Game(teamInd, pairedPotAwayTeamInd),
                                   context)) {
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

bool UECLDraw::DFSStrongCheck(const Game &g, const DFSContext &context) {
    // return true if checks passed; false if any check failed

    // - for each country and each pot pairing, country's home games needed
    //   against the pot pairing and country's away games needed against the pot
    //   pairing must not exceed respective supply
    for (auto &p : teamIndsByCountry) {
        std::string country = p.first;
        std::vector<int> countryTeamInds = p.second;
        for (int pot = 1; pot <= numPots; pot++) {
            // countryHomeNeeds[{country}:{pot}] represents count of country's
            // teams that need home game against pot pair this pot belongs to,
            // and is identical for pots in the same pot pair
            int homeDemand = context.countryHomeNeeds.at(country + ":" +
                                                         std::to_string(pot));

            // conservatively high est of avail slots this pot pair can provide
            // to this country for home games
            int homeSlots = 0;

            // countryAwayNeeds[{country}:{pot}] represents count of country's
            // teams that need away game against pot pair this pot belongs to,
            // and is identical for pots in the same pot pair
            int awayDemand = context.countryAwayNeeds.at(country + ":" +
                                                         std::to_string(pot));

            // conservatively high est of avail slots this pot pair can provide
            // to this country for away games
            int awaySlots = 0;

            int pairedPot = pot % 2 == 0 ? pot - 1 : pot + 1;

            // compute total homeSlots and awaySlots this pot can provide
            for (int i = 0; i < numTeamsPerPot; i++) {
                int potTeamInd = numTeamsPerPot * (pot - 1) + i;
                int pairedPotTeamInd = numTeamsPerPot * (pairedPot - 1) + i;

                // available home slots provided by pot/paired pot teams
                int homeSlotsTeam = 0;
                int pairedPotHomeSlotsTeam = 0;

                // available away slots provided by pot/paired pot teams
                int awaySlotsTeam = 0;
                int pairedPotAwaySlotsTeam = 0;

                // this pot team can contribute up to maxSlotsTeam to pot pair's
                // total home or away slots
                int maxSlotsTeam =
                    2 - get_or(context.numOpponentCountryByTeam,
                               std::to_string(potTeamInd) + ":" + country, 0);
                int pairedPotMaxSlotsTeam =
                    2 - get_or(context.numOpponentCountryByTeam,
                               std::to_string(pairedPotTeamInd) + ":" + country,
                               0);

                // compute # of home slots and away slots this pot/paired pot
                // team can provide
                for (const int &countryTeamInd : countryTeamInds) {
                    if (validRemainingGame(Game(countryTeamInd, potTeamInd),
                                           context)) {
                        homeSlotsTeam =
                            std::min(maxSlotsTeam, homeSlotsTeam + 1);
                    }
                    if (validRemainingGame(Game(potTeamInd, countryTeamInd),
                                           context)) {
                        awaySlotsTeam =
                            std::min(maxSlotsTeam, awaySlotsTeam + 1);
                    }
                    if (validRemainingGame(
                            Game(countryTeamInd, pairedPotTeamInd), context)) {
                        pairedPotHomeSlotsTeam = std::min(
                            pairedPotMaxSlotsTeam, pairedPotHomeSlotsTeam + 1);
                    }
                    if (validRemainingGame(
                            Game(pairedPotTeamInd, countryTeamInd), context)) {
                        pairedPotAwaySlotsTeam = std::min(
                            pairedPotMaxSlotsTeam, pairedPotAwaySlotsTeam + 1);
                    }
                }
                homeSlots += homeSlotsTeam + pairedPotHomeSlotsTeam;
                awaySlots += awaySlotsTeam + pairedPotAwaySlotsTeam;
            }
            if (homeDemand > homeSlots || awayDemand > awaySlots) {
                return false;
            }
        }
    }
    return true;
}