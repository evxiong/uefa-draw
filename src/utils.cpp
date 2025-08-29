#include "utils.h"
#include "globals.h"
#include <fstream>
#include <string>
#include <vector>

std::vector<Team> readCSVTeams(std::string input_file) {
    std::vector<Team> teams;
    std::ifstream file(input_file);
    std::string s;
    std::getline(file, s);
    while (std::getline(file, s)) {
        size_t last = 0;
        size_t next = 0;
        std::vector<std::string> row;
        while ((next = s.find(",", last)) != std::string::npos) {
            row.push_back(s.substr(last, next - last));
            last = next + 1;
        }
        row.push_back(s.substr(last));
        teams.push_back(Team(stoi(row[0]), row[1], row[2], row[3]));
    }
    return teams;
}