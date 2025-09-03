#ifndef SIMULATOR_H
#define SIMULATOR_H

#include "Draw.h"
#include "globals.h"
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

class Simulator {
  public:
    Simulator(int year, std::string competition, std::string input,
              std::string output);
    void run(int iterations);

  private:
    void writeResults() const;
    void updateCounts(const std::vector<Game> &s);

    int failures;
    int year;
    std::string competition; // 'ucl', 'uel', or 'uecl'
    std::string input_file;
    std::filesystem::path output_path;
    std::vector<Team> teams;
    std::unordered_map<std::string, int> counts; // {homeInd}:{awayInd} -> count
};

#endif // SIMULATOR_H