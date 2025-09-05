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
    bool draw(bool suppress = true); // returns false if timeout
    bool draw(BS::light_thread_pool &pool);
    void displayPots() const;
    const std::vector<Game> &getSchedule() const;
    bool verifyDraw(bool suppress = true) const;

  protected:
    Draw(const std::vector<Team> &t, int pots, int teamsPerPot,
         int matchesPerTeam, int matchesPerPotPair);
    Draw(std::string input_path, int pots, int teamsPerPot, int matchesPerTeam,
         int matchesPerPotPair); // input_path: path to teams csv

    void generateAllGames();
    void sortRemainingGames(std::vector<Game> &remainingGames, int sortMode);
    int pickTeamIndex(int pot);
    void updateDrawState(const Game &g, bool revert = false);
    int DFS(const Game &g, const std::vector<Game> &remainingGames,
            const std::chrono::steady_clock::time_point &t0,
            int sortMode); // return values: 0=reject, 1=accept, 2=timeout
    Game pickMatch();
    Game pickMatch(BS::light_thread_pool &pool);

    virtual bool validRemainingGame(const Game &g);
    virtual bool DFSHomeTeamPredicate(int homeTeamIndex, int awayPot);
    virtual bool verifyDrawHomeAway(std::unordered_map<int, DrawVerifier> &m,
                                    int homeTeamIndex, int awayTeamIndex,
                                    bool suppress) const;

    // multithread versions
    DFSContext createDFSContext();
    bool DFS(const Game &g, const std::vector<Game> &remainingGames,
             DFSContext &context, int sortMode, std::atomic<bool> &stop);
    virtual bool validRemainingGame(const Game &g, const DFSContext &context);
    void updateDrawState(const Game &g, DFSContext &context,
                         bool revert = false);
    virtual bool DFSHomeTeamPredicate(int homeTeamIndex, int awayPot,
                                      const DFSContext &context) const;

    // config
    int numPots;
    int numTeamsPerPot;
    int numMatchesPerTeam;
    int numTeams;
    int numMatchesPerPotPair;
    std::vector<Team> teams; // all Teams in draw
    std::mt19937 randomEngine;
    std::unordered_map<std::string, int>
        numTeamsByCountry; // country -> # teams

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
};

class UCLDraw : public Draw {
  public:
    UCLDraw(std::string input_path) : Draw(input_path, 4, 9, 8, 9) {}
    UCLDraw(const std::vector<Team> &t) : Draw(t, 4, 9, 8, 9) {}
};

class UELDraw : public Draw {
  public:
    UELDraw(std::string input_path) : Draw(input_path, 4, 9, 8, 9) {}
    UELDraw(const std::vector<Team> &t) : Draw(t, 4, 9, 8, 9) {}
};

class UECLDraw : public Draw {
  public:
    UECLDraw(std::string input_path);
    UECLDraw(const std::vector<Team> &t);

  protected:
    virtual bool validRemainingGame(const Game &g);
    virtual bool DFSHomeTeamPredicate(int homeTeamIndex, int awayPot);
    virtual bool verifyDrawHomeAway(std::unordered_map<int, DrawVerifier> &m,
                                    int homeTeamIndex, int awayTeamIndex,
                                    bool suppress) const;

    // multithread versions
    virtual bool validRemainingGame(const Game &g, const DFSContext &context);
    virtual bool DFSHomeTeamPredicate(int homeTeamIndex, int awayPot,
                                      const DFSContext &context) const;
};

class TimeoutException : public std::exception {
  public:
    virtual const char *what() const throw() {
        return "DFS timeout exceeded";
    }
};

#endif // DRAW_H