#ifndef DRAW_H
#define DRAW_H

#include "globals.h"
#include <BS_thread_pool/BS_thread_pool.hpp>
#include <atomic>
#include <chrono>
#include <exception>
#include <random>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Draw {
  public:
    bool draw(); // used in debug; returns false if timeout
    bool draw(BS::light_thread_pool &pool); // used in simulations
    void displayPots(bool showCountries = false) const;
    const std::vector<Game> getPickedGames() const;
    bool verifyDraw() const;

  protected:
    Draw(const std::vector<Team> &t, const std::vector<Game> &initialGames,
         const std::unordered_set<std::string> &bannedCountryMatchups, int pots,
         int teamsPerPot, int gamesPerTeam, int gamesPerPotPair, bool suppress);
    Draw(std::string teamsPath, std::string initialGamesPath,
         std::string bannedCountryMatchupsPath, int pots, int teamsPerPot,
         int gamesPerTeam, int gamesPerPotPair, bool suppress);

    void initializeState(
        const std::vector<Game> &initialGames,
        const std::unordered_set<std::string> &bannedCountryMatchups);
    int pickTeamIndex(int pot);
    Game pickGame() const;                            // used in debug
    Game pickGame(BS::light_thread_pool &pool) const; // used in simulations
    bool testCandidateGame(const Game &g,
                           bool strongCheck) const; // used in debug
    bool testCandidateGame(const Game &g, BS::light_thread_pool &pool,
                           bool strongCheck) const; // used in simulations

    virtual void updateDrawState(const Game &g, bool revert = false);
    virtual bool validRemainingGame(const Game &g) const;
    virtual bool verifyDrawHomeAway(std::unordered_map<int, TeamVerifier> &m,
                                    int homeTeamIndex, int awayTeamIndex) const;

    // dfs methods (operate on context independent from obj state)
    DFSContext createDFSContext() const;
    bool dfs(const Game &g, const std::vector<Game> &remainingGames,
             DFSContext &context, int sortMode, bool strongCheck,
             std::atomic<bool> &stop) const;
    void dfsSortRemainingGames(std::vector<Game> &remainingGames,
                               const DFSContext &context, int sortMode) const;
    virtual void dfsUpdateDrawState(const Game &g, DFSContext &context,
                                    bool revert = false) const;
    virtual bool dfsValidRemainingGame(const Game &g,
                                       const DFSContext &context) const;
    virtual bool dfsHomeTeamPredicate(int homeTeamIndex, int awayPot,
                                      const DFSContext &context) const;
    virtual bool dfsWeakCheck(const Game &g, const DFSContext &context) const;
    virtual bool dfsStrongCheck(const DFSContext &context) const;

    // config
    int numPots;
    int numTeamsPerPot;
    int numGamesPerTeam;
    int numTeams;
    int numGamesPerPotPair;
    bool suppress;
    std::vector<Team> teams; // all Teams in draw
    std::mt19937 randomEngine;
    std::unordered_map<std::string, int>
        numTeamsByCountry; // country -> # teams
    std::unordered_map<std::string, std::vector<int>>
        teamIndsByCountry; // country -> team inds

    // current draw state
    std::vector<Game> allGames; // remaining potential Games
    std::vector<Game> pickedGames;
    std::unordered_map<int, std::vector<Game>>
        gamesByTeamInd;                    // team ind -> picked Games
    std::unordered_set<int> drawnTeamInds; // team inds drawn so far
    std::unordered_map<std::string, int>
        numGamesByPotPair; // {home pot}:{away pot} -> # picked games
    std::unordered_map<int, int>
        numHomeGamesByTeamInd; // team ind -> # picked home games
    std::unordered_map<int, int>
        numAwayGamesByTeamInd; // team ind -> # picked away games
    std::unordered_map<std::string, int>
        numGamesByTeamIndOppCountry; // {team ind}:{opp country} -> count
    std::unordered_map<std::string, bool>
        isPickedByTeamIndOppPotLocation; // {team ind}:{opp pot}:{h/a}
    std::unordered_set<std::string>
        pickedGamesTeamInds; // {home team ind}:{away team ind} per picked game

    std::vector<std::unordered_set<int>>
        needsHomeAgainstPot; // pot ind (0-based) -> set of team inds with
                             // unscheduled home games against this pot
    std::vector<std::unordered_set<int>>
        needsAwayAgainstPot; // pot ind (0-based) -> set of team inds with
                             // unscheduled away games against this pot
    std::unordered_map<std::string,
                       int>
        countryHomeNeeds; // {country}:{pot} -> global count of country's teams
                          // that need home game against pot
    std::unordered_map<std::string,
                       int>
        countryAwayNeeds; // {country}:{pot} -> global count of country's teams
                          // that need away game against pot
};

class UCLDraw : public Draw {
  public:
    UCLDraw(std::string teamsPath, std::string initialGamesPath = "",
            std::string bannedCountryMatchupsPath = "", bool suppress = true)
        : Draw(teamsPath, initialGamesPath, bannedCountryMatchupsPath, 4, 9, 8,
               9, suppress) {}
    UCLDraw(const std::vector<Team> &t,
            const std::vector<Game> &g = std::vector<Game>(),
            const std::unordered_set<std::string> &bc =
                std::unordered_set<std::string>(),
            bool suppress = true)
        : Draw(t, g, bc, 4, 9, 8, 9, suppress) {}
};

class UELDraw : public Draw {
  public:
    UELDraw(std::string teamsPath, std::string initialGamesPath = "",
            std::string bannedCountryMatchupsPath = "", bool suppress = true)
        : Draw(teamsPath, initialGamesPath, bannedCountryMatchupsPath, 4, 9, 8,
               9, suppress) {}
    UELDraw(const std::vector<Team> &t,
            const std::vector<Game> &g = std::vector<Game>(),
            const std::unordered_set<std::string> &bc =
                std::unordered_set<std::string>(),
            bool suppress = true)
        : Draw(t, g, bc, 4, 9, 8, 9, suppress) {}
};

class UECLDraw : public Draw {
  public:
    UECLDraw(std::string teamsPath, std::string initialGamesPath = "",
             std::string bannedCountryMatchupsPath = "", bool suppress = true);
    UECLDraw(const std::vector<Team> &t,
             const std::vector<Game> &g = std::vector<Game>(),
             const std::unordered_set<std::string> &bc =
                 std::unordered_set<std::string>(),
             bool suppress = true);

  protected:
    virtual void updateDrawState(const Game &g, bool revert = false);
    virtual bool validRemainingGame(const Game &g) const;
    virtual bool verifyDrawHomeAway(std::unordered_map<int, TeamVerifier> &m,
                                    int homeTeamIndex, int awayTeamIndex) const;

    // multithread versions
    virtual void dfsUpdateDrawState(const Game &g, DFSContext &context,
                                    bool revert = false) const;
    virtual bool dfsValidRemainingGame(const Game &g,
                                       const DFSContext &context) const;
    virtual bool dfsHomeTeamPredicate(int homeTeamIndex, int awayPot,
                                      const DFSContext &context) const;
    virtual bool dfsWeakCheck(const Game &g, const DFSContext &context) const;
    virtual bool dfsStrongCheck(const DFSContext &context) const;
};

class TimeoutException : public std::exception {
  public:
    virtual const char *what() const throw() {
        return "DFS timeout exceeded";
    }
};

#endif // DRAW_H