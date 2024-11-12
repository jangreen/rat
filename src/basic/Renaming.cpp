#include "Renaming.h"

#include <boost/container/flat_set.hpp>
#include <unordered_set>

const auto projFirst = &std::pair<int, int>::first;
const auto projSecond = &std::pair<int, int>::second;

Renaming::Renaming(Mapping&& map) : mapping(std::move(map)) {
  assert(std::ranges::is_sorted(mapping, std::less(), projFirst) && "domain is unsorted");
  assert(std::ranges::adjacent_find(mapping, std::equal_to(), projFirst) == mapping.end() &&
         "duplicates in domain");
}

Renaming Renaming::empty() { return Renaming({}); }

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

Renaming Renaming::identity(const boost::container::flat_set<int>& domain) {
  Mapping mapping;
  mapping.reserve(domain.size());
  for (auto i : domain) {
    mapping.emplace_back(i, i);
  }
  std::ranges::sort(mapping, std::less<int>{}, projFirst);
  return Renaming(std::move(mapping));
}

size_t Renaming::size() const { return mapping.size(); }

Renaming Renaming::inverted() const {
  assert(({
           boost::container::flat_set<int> rangeSet;
           rangeSet.reserve(mapping.size());
           std::ranges::for_each(mapping, [&](const auto x) { rangeSet.insert(x); }, projSecond);
           rangeSet.size() == mapping.size();
         }) &&
         "duplicates in range");
  Mapping inverted;
  inverted.reserve(mapping.size());
  for (auto [from, to] : mapping) {
    inverted.emplace_back(to, from);
  }
  std::ranges::sort(inverted, std::less<int>{}, projFirst);
  return Renaming(std::move(inverted));
}

Renaming Renaming::strictCompose(const Renaming& other) const {
  Mapping composed;
  composed.reserve(mapping.size());
  for (auto [a, b] : mapping) {
    if (auto c = other.applyStrict(b); c.has_value()) {
      composed.emplace_back(a, c.value());
    }
  }
  return Renaming(std::move(composed));
}

Renaming Renaming::compose(const Renaming& other) const {
  Mapping composed;
  composed.reserve(mapping.size());
  for (auto [a, b] : mapping) {
    composed.emplace_back(a, other.apply(b));
  }
  return Renaming(std::move(composed));
}

Renaming Renaming::totalCompose(const Renaming& other) const {
  Mapping composed;
  composed.reserve(mapping.size() + other.size());
  for (auto [a, b] : mapping) {
    composed.emplace_back(a, other.apply(b));
  }
  for (auto [a, b] : other.mapping) {
    if (std::ranges::find(mapping, a, projFirst) == mapping.end()) {
      composed.emplace_back(a, b);
    }
  }
  return Renaming(std::move(composed));
}

int Renaming::apply(const int n) const { return applyStrict(n).value_or(n); }

std::optional<int> Renaming::applyStrict(const int n) const {
  const auto iter = std::ranges::find(mapping, n, &std::pair<int, int>::first);
  return iter == mapping.end() ? std::nullopt : std::optional(iter->second);
}

void Renaming::toDotFormat(std::ofstream& output) const {
  for (auto [from, to] : mapping) {
    output << from << " -> " << to << "\n";
  }
}
