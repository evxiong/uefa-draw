#ifndef GLOBALS_H
#define GLOBALS_H

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Terminal colors
#define RESET "\033[0m"
#define RED "\033[31m"     /* Red */
#define YELLOW "\033[33m"  /* Yellow */
#define GREEN "\033[32m"   /* Green */
#define BLUE "\033[34m"    /* Blue */
#define CYAN "\033[36m"    /* Cyan */
#define MAGENTA "\033[35m" /* Magenta */
#define BLACK "\033[30m"   /* Black */
#define BOLD "\033[1m"     /* Bold */

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
    Game(const Game &rhs) {
        h = rhs.h;
        a = rhs.a;
    }
};

struct DrawVerifier {
    std::unordered_set<int> opponents;
    std::unordered_map<std::string, int> opponentCountryCount;
    std::unordered_map<std::string, bool>
        hasPlayedWithPot; // {opponent_pot}:{h/a}
    std::unordered_map<int, int> numMatchesPerPot;
};

#endif // GLOBALS_H