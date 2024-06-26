#pragma once
#include <fstream>
#include <vector>

class Renaming {
 public:
  Renaming(std::vector<int> from);
  Renaming(int from, int to);
  Renaming() = default;

  std::vector<int> from = {};
  std::vector<int> to = {};

  int rename(int n) const {
    auto index = std::ranges::find(from, n);
    if (index == from.end()) {
      return n;
    }
    return to.at(std::distance(from.begin(), index));
  }
  void invert() { swap(from, to); };
  size_t size() const { return from.size(); };

  void toDotFormat(std::ofstream &output) const {
    for (size_t i = 0; i < from.size(); i++) {
      output << from.at(i) << " -> " << to.at(i) << "\n";
    }
  }
};
