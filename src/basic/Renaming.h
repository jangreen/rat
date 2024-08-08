#pragma once
#include <algorithm>
#include <boost/container/flat_set.hpp>
#include <fstream>
#include <optional>
#include <ranges>
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
  static Renaming empty() { return Renaming({}); }
  static Renaming minimal(const std::vector<int> &from);
  static Renaming simple(int from, int to);
  static Renaming identity(const boost::container::flat_set<int> &domain);

  [[nodiscard]] Renaming inverted() const;
  // (0->1,1->0) ; (2->3,1->0) = 0->0     no implicit identity
  [[nodiscard]] Renaming strictCompose(const Renaming &other) const;
  // (0->1,1->0) ; (2->3,1->0) = (0->0,1->0)    second implicit identity
  [[nodiscard]] Renaming compose(const Renaming &other) const;
  // (0->1,1->0) ; (2->3,1->0) = (0->0,1->0,2->3)     both implicit identity
  [[nodiscard]] Renaming totalCompose(const Renaming &other) const;

  [[nodiscard]] size_t size() const { return mapping.size(); }
  [[nodiscard]] const std::vector<std::pair<int, int>> &getMapping() const { return mapping; }

  [[nodiscard]] std::optional<int> renameStrict(const int n) const {
    const auto iter = std::ranges::find(mapping, n, &std::pair<int, int>::first);
    return iter == mapping.end() ? std::nullopt : std::optional(iter->second);
  }

  template <class Container>
  [[nodiscard]] bool isStrictlyRenameable(const Container &toRename) const {
    return std::ranges::all_of(toRename, [&](const auto x) { return renameStrict(x).has_value(); });
  }

  [[nodiscard]] int rename(const int n) const { return renameStrict(n).value_or(n); }

  void toDotFormat(std::ofstream &output) const {
    for (auto [from, to] : mapping) {
      output << from << " -> " << to << "\n";
    }
  }
};
