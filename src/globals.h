#ifndef GLOBALS_H
#define GLOBALS_H

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Terminal colors
#define RESET "\033[0m"
#define GRAY "\033[90m"
#define RED "\033[91m"
#define BLUE "\033[94m"
#define GREEN "\033[92m"
#define YELLOW "\033[93m"
#define CYAN "\033[96m"
#define MAGENTA "\033[95m"

extern const std::string POT_COLORS[];

struct Team {
    int pot; // 1-based
    std::string abbrev;
    std::string country;
    std::string name;
    float coefficient = 0;
    Team(int p, std::string a, std::string c, std::string n)
        : pot(p), abbrev(a), country(c), name(n) {}
    Team(int p, std::string a, std::string c, std::string n, float coeff)
        : pot(p), abbrev(a), country(c), name(n), coefficient(coeff) {}
};

struct Game {
    int h; // home team index (0-based)
    int a; // away team index (0-based)
    Game(int home, int away) : h(home), a(away) {}
    bool operator==(const Game &rhs) {
        return (h == rhs.h && a == rhs.a);
    }
};

// used to verify each team after draw
struct TeamVerifier {
    std::unordered_set<int> oppTeamInds;
    std::unordered_map<std::string, int> numOppsByCountry;
    std::unordered_map<std::string, bool>
        isPickedByOppPotLocation; // {opp pot}:{h/a}
    std::unordered_map<int, int> numGamesByPot;
};

// current draw state, used in DFS
struct DFSContext {
    std::vector<Game> pickedGames;
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
        needsHomeAgainstPot; // pot ind (0-based) -> set of team inds
                             // with unscheduled home games against
                             // this pot
    std::vector<std::unordered_set<int>>
        needsAwayAgainstPot; // pot ind (0-based) -> set of team inds
                             // with unscheduled away games against
                             // this pot
    std::unordered_map<std::string,
                       int>
        countryHomeNeeds; // {country}:{pot} -> global count of country's teams
                          // that need home game against pot
    std::unordered_map<std::string,
                       int>
        countryAwayNeeds; // {country}:{pot} -> global count of country's teams
                          // that need away game against pot
};

#endif // GLOBALS_H