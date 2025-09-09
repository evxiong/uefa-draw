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

const int NUM_POTS = 4;
const int NUM_TEAMS_PER_POT = 9;
const int NUM_MATCHES_PER_TEAM = 8;
const int NUM_TEAMS = NUM_POTS * NUM_TEAMS_PER_POT;

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

struct DrawVerifier {
    std::unordered_set<int> opponents;
    std::unordered_map<std::string, int> opponentCountryCount;
    std::unordered_map<std::string, bool>
        hasPlayedWithPot; // {opponent_pot}:{h/a}
    std::unordered_map<int, int> numMatchesPerPot;
};

// current draw state, used in DFS
struct DFSContext {
    std::vector<Game> pickedMatches;
    std::unordered_map<std::string, int>
        numGamesByPotPair; // {home pot}:{away pot} -> # games
    std::unordered_map<int, int> numHomeGamesByTeam; // team ind -> # home games
    std::unordered_map<int, int> numAwayGamesByTeam; // team ind -> # away games
    std::unordered_map<std::string, int>
        numOpponentCountryByTeam; // {team ind}:{opp country} -> count
    std::unordered_map<std::string, bool>
        hasPlayedWithPotMap; // {team ind}:{opp pot}:{h/a dep. on team ind}
    std::unordered_set<std::string>
        pickedMatchesTeamIndices; // {home team ind}:{away team ind}
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