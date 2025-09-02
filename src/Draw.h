#ifndef DRAW_H
#define DRAW_H

#include "globals.h"
#include <chrono>
#include <exception>
#include <random>
#include <string>
#include <vector>

class Draw {
  public:
    bool draw(bool suppress = true); // returns false if timeout
    void displayPots() const;
    const std::vector<Game> &getSchedule() const;

    virtual bool verifyDraw(bool suppress = true) const;

  protected:
    Draw(const std::vector<Team> &t, int pots, int teamsPerPot,
         int matchesPerTeam);
    Draw(std::string input_path, int pots, int teamsPerPot,
         int matchesPerTeam); // input_path: path to teams csv
    void generateAllGames();
    void sortRemainingGames(int sortMode, std::vector<Game> &remainingGames);
    bool testCandidateGame(const Game &cG);
    void drawTeam(int pot, bool suppress);
    int pickTeamIndex(int pot);
    virtual Game pickMatch();
    void updateDrawState(const Game &g, bool revert = false);

    virtual bool validRemainingGame(const Game &g, const Game &aG);
    virtual int DFS(const Game &g, const std::vector<Game> &remainingGames,
                    int depth, const std::chrono::steady_clock::time_point &t0,
                    int sortMode);
    // return values: 0=reject, 1=accept, 2=timeout

    // config
    int numPots;
    int numTeamsPerPot;
    int numMatchesPerTeam;
    int numTeams;
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
    UCLDraw(std::string input_path) : Draw(input_path, 4, 9, 8) {}
    UCLDraw(const std::vector<Team> &t) : Draw(t, 4, 9, 8) {}
};

class UELDraw : public Draw {
  public:
    UELDraw(std::string input_path) : Draw(input_path, 4, 9, 8) {}
    UELDraw(const std::vector<Team> &t) : Draw(t, 4, 9, 8) {}
};

class UECLDraw : public Draw {
  public:
    UECLDraw(std::string input_path);
    UECLDraw(const std::vector<Team> &t);
    virtual bool verifyDraw(bool suppress = true) const;

  protected:
    // virtual Game pickMatch();
    virtual bool validRemainingGame(const Game &g, const Game &aG);
    // virtual bool DFS(const Game &g, const std::vector<Game> &remainingGames,
    //                  int depth,
    //                  const std::chrono::steady_clock::time_point &t0);
};

class TimeoutException : public std::exception {
  public:
    virtual const char *what() const throw() {
        return "DFS timeout exceeded";
    }
};

#endif // DRAW_H