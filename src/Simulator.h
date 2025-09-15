#ifndef SIMULATOR_H
#define SIMULATOR_H

#include "Draw.h"
#include "globals.h"
#include <chrono>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

class Simulator {
  public:
    Simulator(int year, std::string competition, std::string teamsPath = "");
    void run(int iterations, std::string output = "") const;

  private:
    void writeResults(const std::unordered_map<std::string, int> &counts,
                      const std::filesystem::path &outputPath,
                      const std::chrono::system_clock::time_point &tp,
                      int iterations) const;

    int year;
    std::string competition; // 'ucl', 'uel', or 'uecl'
    std::vector<Team> teams;
};

#endif // SIMULATOR_H