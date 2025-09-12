#include "utils.h"
#include "globals.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

std::vector<Team> readCSVTeams(std::string input_file) {
    std::vector<Team> teams;
    std::ifstream file(input_file);
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

std::vector<Game> readTXTGames(std::string input_file,
                               const std::vector<Team> &teams) {
    std::unordered_map<std::string, int> teamIndexByAbbrev;
    for (size_t i = 0; i < teams.size(); i++) {
        teamIndexByAbbrev[teams[i].abbrev] = static_cast<int>(i);
    }

    std::vector<Game> games;
    std::ifstream file(input_file);
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