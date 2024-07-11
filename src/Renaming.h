#pragma once
#include <boost/container/flat_set.hpp>
#include <fstream>
#include <vector>

typedef boost::container::flat_set<int> IntSet;

class Renaming {
 public:
  explicit Renaming(const std::vector<int> &from);
  Renaming(int from, int to);
  Renaming() = default;

  std::vector<int> from = {};
  std::vector<int> to = {};

  [[nodiscard]] int rename(const int n) const {
    const auto index = std::ranges::find(from, n);
    if (index == from.end()) {
      return n;
    }
    return to.at(std::distance(from.begin(), index));
  }
  void invert() { swap(from, to); };
  [[nodiscard]] Renaming compose(const Renaming &other) const {
    IntSet potentialDomain;
    potentialDomain.insert(from.begin(), from.end());
    potentialDomain.insert(other.from.begin(), other.from.end());

    Renaming composedRenaming;
    composedRenaming.from.reserve(potentialDomain.size());
    composedRenaming.to.reserve(potentialDomain.size());

    for (auto potentialFrom : potentialDomain) {
      const auto potentialTo = other.rename(rename(potentialFrom));

      if (potentialFrom != potentialTo) {
        composedRenaming.from.push_back(potentialFrom);
        composedRenaming.to.push_back(potentialTo);
      }
    }
    return composedRenaming;
  }
  [[nodiscard]] size_t size() const { return from.size(); };

  void toDotFormat(std::ofstream &output) const {
    for (size_t i = 0; i < from.size(); i++) {
      output << from.at(i) << " -> " << to.at(i) << "\n";
    }
  }
};
