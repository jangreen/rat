#pragma once
#include <boost/container/flat_set.hpp>
#include <fstream>
#include <vector>

/*
 * A Renaming is a partial permutation int -> int.
 */
class Renaming {
private:
  typedef std::vector<std::pair<int, int>> Mapping;
  explicit Renaming(Mapping &&map);

  Mapping mapping;
 public:
  static Renaming minimal(const std::vector<int> &from);
  static Renaming simple(int from, int to);

  [[nodiscard]] Renaming inverted() const;
  [[nodiscard]] Renaming compose(const Renaming &other) const;

  [[nodiscard]] size_t size() const { return mapping.size(); }
  [[nodiscard]] const std::vector<std::pair<int, int>>& getMapping() const { return mapping; }

  [[nodiscard]] std::optional<int> renameStrict(const int n) const {
    const auto iter = std::ranges::find(mapping, n, &std::pair<int, int>::first);
    return iter == mapping.end() ? std::nullopt : std::optional(iter->second) ;
  }

  template <class Container>
  [[nodiscard]] bool isStrictlyRenameable(const Container& toRename) const {
    return std::ranges::all_of(toRename, [&](const auto x) { return renameStrict(x).has_value(); });
  }

  [[nodiscard]] int rename(const int n) const {
    return renameStrict(n).value_or(n);
  }

  void toDotFormat(std::ofstream &output) const {
    for (auto [from, to] : mapping) {
      output << from << " -> " << to << "\n";
    }
  }

};

