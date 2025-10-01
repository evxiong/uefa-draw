#include "utils.h"
#include "globals.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

std::vector<Team> readCSVTeams(std::string path) {
    std::vector<Team> teams;
    std::ifstream file(path);
    std::string line;
    std::getline(file, line);
    while (std::getline(file, line)) {
        if (trim(line).empty()) {
            continue;
        }
        std::stringstream ss(line);
        std::string field;
        std::vector<std::string> row;
        while (std::getline(ss, field, ',')) {
            row.push_back(field);
        }
        teams.push_back(Team(std::stoi(row[0]), row[1], row[2], row[3]));
    }
    return teams;
}

std::vector<Game> readTXTGames(std::string path,
                               const std::vector<Team> &teams) {
    std::unordered_map<std::string, int> teamIndexByAbbrev;
    for (size_t i = 0; i < teams.size(); i++) {
        teamIndexByAbbrev[teams[i].abbrev] = static_cast<int>(i);
    }

    std::vector<Game> games;
    std::ifstream file(path);
    std::string line;
    while (std::getline(file, line)) {
        if (trim(line).empty()) {
            continue;
        }
        size_t pos = line.find('-');
        std::string home = trim(line.substr(0, pos));
        std::string away = trim(line.substr(pos + 1));
        games.emplace_back(teamIndexByAbbrev.at(home),
                           teamIndexByAbbrev.at(away));
    }
    return games;
}

std::unordered_set<std::string> readTXTCountries(std::string path) {
    std::unordered_set<std::string> countryMatchups;
    std::ifstream file(path);
    std::string line;
    while (std::getline(file, line)) {
        if (trim(line).empty()) {
            continue;
        }
        size_t pos = line.find('-');
        std::string country1 = trim(line.substr(0, pos));
        std::string country2 = trim(line.substr(pos + 1));
        countryMatchups.insert(country1 + ":" + country2);
    }
    return countryMatchups;
}

std::string trim(const std::string &s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end = s.find_last_not_of(" \t\r\n");

    if (start == std::string::npos) {
        // string is all whitespace
        return "";
    }
    return s.substr(start, end - start + 1);
};

std::string toLower(const std::string &s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

std::string
formatSystemTimePoint(const std::chrono::system_clock::time_point &tp,
                      std::string format) {
    std::time_t tt = std::chrono::system_clock::to_time_t(tp);
    std::tm *tm = std::localtime(&tt);
    std::ostringstream oss;
    oss << std::put_time(tm, format.c_str());
    return oss.str();
}