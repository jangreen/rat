#include "Renaming.h"

#include <boost/container/flat_set.hpp>
#include <unordered_set>

const auto projFirst = &std::pair<int, int>::first;
const auto projSecond = &std::pair<int, int>::second;

// Private constructor
Renaming::Renaming(Mapping&& map) : mapping(std::move(map)) {
  assert(std::ranges::is_sorted(mapping, std::less(), projFirst) && "domain is unsorted");
  assert(std::ranges::adjacent_find(mapping, std::equal_to(), projFirst) == mapping.end() &&
         "duplicates in domain");
  assert(({
           boost::container::flat_set<int> rangeSet;
           rangeSet.reserve(mapping.size());
           std::ranges::for_each(mapping, [&](const auto x) { rangeSet.insert(x); }, projSecond);
           rangeSet.size() == mapping.size();
         }) &&
         "duplicates in range");
}

Renaming Renaming::minimal(const std::vector<int>& from) {
  Mapping mapping;
  mapping.reserve(from.size());
  for (int i = 0; i < from.size(); i++) {
    mapping.emplace_back(from[i], i);
  }
  std::ranges::sort(mapping, std::less<int>{}, projFirst);
  return Renaming(std::move(mapping));
}

Renaming Renaming::simple(int from, int to) { return Renaming({{from, to}}); }

Renaming Renaming::inverted() const {
  Mapping inverted;
  inverted.reserve(mapping.size());
  for (auto [from, to] : mapping) {
    inverted.emplace_back(to, from);
  }
  std::ranges::sort(inverted, std::less<int>{}, projFirst);
  return Renaming(std::move(inverted));
}

Renaming Renaming::compose(const Renaming& other) const {
  Mapping composed;
  composed.reserve(mapping.size());
  for (auto [a, b] : mapping) {
    if (auto c = other.renameStrict(b); c.has_value()) {
      composed.emplace_back(a, c.value());
    }
  }
  return Renaming(std::move(composed));
}

Renaming Renaming::totalCompose(const Renaming& other) const {
  Mapping composed;
  composed.reserve(mapping.size());
  for (auto [a, b] : mapping) {
    composed.emplace_back(a, other.rename(b));
  }
  return Renaming(std::move(composed));
}
