#ifndef DRAW_H
#define DRAW_H

#include "globals.h"
#include <BS_thread_pool/BS_thread_pool.hpp>
#include <atomic>
#include <chrono>
#include <exception>
#include <random>
#include <string>
#include <vector>

class Draw {
  public:
    bool draw(); // returns false if timeout
    bool draw(BS::light_thread_pool &pool);
    void displayPots(bool showCountries = false) const;
    const std::vector<Game> getPickedMatches() const;
    bool verifyDraw() const;

  protected:
    Draw(const std::vector<Team> &t, const std::vector<Game> &initialMatchState,
         int pots, int teamsPerPot, int matchesPerTeam, int matchesPerPotPair,
         bool suppress);
    Draw(std::string inputTeamsPath, std::string inputMatchesPath, int pots,
         int teamsPerPot, int matchesPerTeam, int matchesPerPotPair,
         bool suppress);

    void initializeState(const std::vector<Game> &initialMatchState);
    void sortRemainingGames(std::vector<Game> &remainingGames, int sortMode);
    void sortRemainingGames(std::vector<Game> &remainingGames,
                            const DFSContext &context, int sortMode);
    int pickTeamIndex(int pot);
    int DFS(const Game &g, const std::vector<Game> &remainingGames,
            const std::chrono::steady_clock::time_point &t0,
            int sortMode); // return values: 0=reject, 1=accept, 2=timeout
    Game pickMatch();
    Game pickMatch(BS::light_thread_pool &pool);

    virtual void updateDrawState(const Game &g, bool revert = false);
    virtual bool validRemainingGame(const Game &g);
    virtual bool DFSHomeTeamPredicate(int homeTeamIndex, int awayPot);
    virtual bool verifyDrawHomeAway(std::unordered_map<int, DrawVerifier> &m,
                                    int homeTeamIndex, int awayTeamIndex) const;

    // multithread versions
    DFSContext createDFSContext();
    bool testCandidateGame(const Game &g, BS::light_thread_pool &pool,
                           bool strongCheck);
    bool DFS(const Game &g, const std::vector<Game> &remainingGames,
             DFSContext &context, int sortMode, bool strongCheck,
             std::atomic<bool> &stop);
    virtual bool validRemainingGame(const Game &g, const DFSContext &context);
    virtual void updateDrawState(const Game &g, DFSContext &context,
                                 bool revert = false);
    virtual bool DFSHomeTeamPredicate(int homeTeamIndex, int awayPot,
                                      const DFSContext &context) const;
    virtual bool DFSWeakCheck(const Game &g, const DFSContext &context);
    virtual bool DFSStrongCheck(const Game &g, const DFSContext &context);

    // config
    int numPots;
    int numTeamsPerPot;
    int numMatchesPerTeam;
    int numTeams;
    int numMatchesPerPotPair;
    bool suppress;
    std::vector<Team> teams; // all Teams in draw
    std::mt19937 randomEngine;
    std::unordered_map<std::string, int>
        numTeamsByCountry; // country -> # teams
    std::unordered_map<std::string, std::vector<int>>
        teamIndsByCountry; // country -> team inds

    // current draw state
    std::vector<Game> allGames;      // remaining potential Games
    std::vector<Game> pickedMatches; // picked Games
    std::unordered_map<int, std::vector<Game>>
        gamesByTeam; // team ind -> picked Games
    std::unordered_map<std::string, int>
        numGamesByPotPair; // {home pot}:{away pot} -> # games
    std::unordered_map<int, int> numHomeGamesByTeam; // team ind -> # home games
    std::unordered_map<int, int> numAwayGamesByTeam; // team ind -> # away games
    std::unordered_map<std::string, int>
        numOpponentCountryByTeam; // {team ind}:{opp country} -> count
    std::unordered_map<std::string, bool>
        hasPlayedWithPotMap; // {team ind}:{opp pot}:{h/a dep. on team ind}
    std::unordered_set<std::string>
        pickedMatchesTeamIndices;             // {home team ind}:{away team ind}
    std::unordered_set<int> drawnTeamIndices; // team inds drawn so far

    std::vector<std::unordered_set<int>>
        needsHomeAgainstPot; // pot ind -> set of team inds with
                             // unscheduled home games against this pot
    std::vector<std::unordered_set<int>>
        needsAwayAgainstPot; // pot ind -> set of team inds with
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
    UCLDraw(std::string inputTeamsPath, std::string inputMatchesPath,
            bool suppress = true)
        : Draw(inputTeamsPath, inputMatchesPath, 4, 9, 8, 9, suppress) {}
    UCLDraw(const std::vector<Team> &t,
            const std::vector<Game> &m = std::vector<Game>(),
            bool suppress = true)
        : Draw(t, m, 4, 9, 8, 9, suppress) {}
};

class UELDraw : public Draw {
  public:
    UELDraw(std::string inputTeamsPath, std::string inputMatchesPath,
            bool suppress = true)
        : Draw(inputTeamsPath, inputMatchesPath, 4, 9, 8, 9, suppress) {}
    UELDraw(const std::vector<Team> &t,
            const std::vector<Game> &m = std::vector<Game>(),
            bool suppress = true)
        : Draw(t, m, 4, 9, 8, 9, suppress) {}
};

class UECLDraw : public Draw {
  public:
    UECLDraw(std::string inputTeamsPath, std::string inputMatchesPath,
             bool suppress = true);
    UECLDraw(const std::vector<Team> &t,
             const std::vector<Game> &m = std::vector<Game>(),
             bool suppress = true);

  protected:
    virtual void updateDrawState(const Game &g, bool revert = false);
    virtual bool validRemainingGame(const Game &g);
    virtual bool DFSHomeTeamPredicate(int homeTeamIndex, int awayPot);
    virtual bool verifyDrawHomeAway(std::unordered_map<int, DrawVerifier> &m,
                                    int homeTeamIndex, int awayTeamIndex) const;

    // multithread versions
    virtual void updateDrawState(const Game &g, DFSContext &context,
                                 bool revert = false);
    virtual bool validRemainingGame(const Game &g, const DFSContext &context);
    virtual bool DFSHomeTeamPredicate(int homeTeamIndex, int awayPot,
                                      const DFSContext &context) const;
    virtual bool DFSWeakCheck(const Game &g, const DFSContext &context);
    virtual bool DFSStrongCheck(const Game &g, const DFSContext &context);
};

class TimeoutException : public std::exception {
  public:
    virtual const char *what() const throw() {
        return "DFS timeout exceeded";
    }
};

#endif // DRAW_H