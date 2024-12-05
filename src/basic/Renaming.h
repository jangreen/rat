#pragma once
#include <algorithm>
#include <boost/container/flat_set.hpp>
#include <fstream>
#include <optional>
#include <vector>

#include "../helper/range_of.h"

/*
 * A Renaming is a partial permutation int -> int.
 */
class Renaming {
 private:
  typedef std::vector<std::pair<int, int>> Mapping;
  explicit Renaming(Mapping &&map);

  Mapping mapping;

 public:
  static Renaming empty();
  static Renaming minimal(const std::vector<int> &from);
  static Renaming simple(int from, int to);
  static Renaming identity(const boost::container::flat_set<int> &domain);

  [[nodiscard]] size_t size() const;
  [[nodiscard]] Renaming inverted() const;

  // - composition
  // strict(no implicit identity): (0->1,1->0) ; (2->3,1->0) = 0->0
  // compose(second implicit identity): (0->1,1->0) ; (2->3,1->0) = (0->0,1->0)
  // total(both implicit identity): (0->1,1->0) ; (2->3,1->0) = (0->0,1->0,2->3)
  [[nodiscard]] Renaming strictCompose(const Renaming &other) const;
  [[nodiscard]] Renaming compose(const Renaming &other) const;
  [[nodiscard]] Renaming totalCompose(const Renaming &other) const;

  // - renaming application (strict = no implicit identity)
  [[nodiscard]] int apply(int n) const;
  [[nodiscard]] std::optional<int> applyStrict(int n) const;

  [[nodiscard]] bool isStrictlyRenameable(const range_of<int> auto &toRename) const;

  void toDotFormat(std::ofstream &output) const;
};

bool Renaming::isStrictlyRenameable(const range_of<int> auto &toRename) const {
  return std::ranges::all_of(toRename, [&](const int x) { return applyStrict(x).has_value(); });
}
