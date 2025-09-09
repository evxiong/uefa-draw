#ifndef UTILS_H
#define UTILS_H

#include "globals.h"
#include <string>
#include <unordered_map>
#include <vector>

std::vector<Team> readCSVTeams(std::string input_file);
std::vector<Game> readTXTGames(std::string input_file,
                               const std::vector<Team> &teams);
std::string trim(const std::string &s);
std::string toLower(const std::string &s);

// Get `key`'s value from map; return `default_value` if `key` not found
template <typename Map, typename Key,
          typename Value = typename Map::mapped_type>
Value get_or(const Map &m, const Key &key, const Value &default_value) {
    auto it = m.find(key);
    if (it != m.end()) {
        return it->second;
    }
    return default_value;
}

#endif // UTILS_H