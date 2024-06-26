#pragma once
#include <fstream>
#include <vector>

class Renaming {
 public:
  Renaming(std::vector<int> from);
  Renaming(std::vector<int> from, std::vector<int> to);
  Renaming(int from, int to);
  Renaming() = default;

  std::vector<int> from = {};
  std::vector<int> to = {};

  int rename(int n) const { return to[std::distance(from.begin(), std::ranges::find(from, n))]; }
  void invert() { swap(from, to); };
  int size() const { return from.size(); };

  void toDotFormat(std::ofstream &output) const {
    for (size_t i = 0; i < from.size(); i++) {
      output << from.at(i) << "." << to.at(i) << ", ";
    }
  }
};
