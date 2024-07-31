#pragma once

#include <iostream>
#include <ranges>
#include <unordered_set>

#include "Stats.h"
#include "basic/Literal.h"

// currently it is not possible to pass a concept as a template parameter
// we would like to write something like this: range_of<Literal>, range_of<rane_of<Literal>>, ...
// template <typename RangeType, template <typename> concept ValueConcept>
// concept range_of =
//     std::ranges::range<RangeType> && ValueConcept<std::ranges::range_value_t<RangeType>>;
// workaround:
template <class RangeType, class RangeValueType>
concept range_of = std::ranges::range<RangeType> &&
                   std::same_as<RangeValueType, std::ranges::range_value_t<RangeType>>;
template <class RangeType, class RangeValueType>
concept range_of_range_of = std::ranges::range<RangeType> &&
                            range_of<std::ranges::range_value_t<RangeType>, RangeValueType>;

template <typename T>
bool isSubset(const std::vector<T> &smallerSet, std::vector<T> largerSet) {
  std::unordered_set<T> set(std::make_move_iterator(largerSet.begin()),
                            std::make_move_iterator(largerSet.end()));
  return std::ranges::all_of(smallerSet, [&](auto &element) { return set.contains(element); });
}

template <typename T>
std::vector<T> flatten(const std::vector<std::vector<T>> &orig) {
  std::vector<T> ret;
  for (const auto &v : orig) ret.insert(ret.end(), v.begin(), v.end());
  return ret;
}

void print(const range_of_range_of<Literal> auto &dnf) {
  std::cout << "Cubes:";
  for (auto &cube : dnf) {
    std::cout << "\n";
    for (auto &literal : cube) {
      std::cout << literal.toString() << " , ";
    }
  }
  std::cout << std::endl;
}

void print(const range_of<Literal> auto &cube) {
  for (auto &literal : cube) {
    std::cout << literal.toString() << "\n";
  }
  std::cout << std::endl;
}

inline std::string toString(const EventSet &events) {
  std::string output;
  for (auto &event : events) {
    output += std::to_string(event) + ", ";
  }
  return output;
}

inline std::string toString(const SetOfSets &sets) {
  std::string output;
  for (auto &set : sets) {
    output += set->toString() + ", ";
  }
  return output;
}

// Dirty hack to access internals of adapters like priority_queue
template <class ADAPTER>
typename ADAPTER::container_type &get_container(ADAPTER &a) {
  struct hack : ADAPTER {
    static typename ADAPTER::container_type &get(ADAPTER &a) { return a.*&hack::c; }
  };
  return hack::get(a);
}

template <class ADAPTER>
const typename ADAPTER::container_type &get_const_container(ADAPTER &a) {
  struct hack : ADAPTER {
    const static typename ADAPTER::container_type &get(ADAPTER &a) { return a.*&hack::c; }
  };
  return hack::get(a);
}

inline bool validateCube(const Cube &cube) {
  Cube copy = cube;
  std::ranges::sort(copy);
  const bool hasDuplicates = std::ranges::adjacent_find(copy) != copy.end();
  assert(!hasDuplicates);
  return std::ranges::all_of(cube, [](const auto &literal) { return literal.validate(); });
}

inline void renameCube(const Renaming &renaming, Cube &cube) {
  for (auto &literal : cube) {
    literal.rename(renaming);
  }
}

inline bool validatePartialCube(const PartialCube &cube) {
  return std::ranges::all_of(cube, [](const PartialLiteral &literal) {
    if (std::holds_alternative<Literal>(literal)) {
      return std::get<Literal>(literal).validate();
    } else {
      return Annotated::validate(std::get<AnnotatedSet>(literal));
    }
  });
}

inline bool validateNormalizedCube(const Cube &cube) {
  return validateCube(cube) && std::ranges::all_of(cube, [](auto &lit) { return lit.isNormal(); });
}

inline bool validateDNF(const DNF &dnf) {
  return std::ranges::all_of(dnf, [](const auto &cube) { return validateCube(cube); });
}

inline bool validatePartialDNF(const PartialDNF &dnf) {
  return std::ranges::all_of(dnf,
                             [](const PartialCube &cube) { return validatePartialCube(cube); });
}

template <typename T>
bool contains(const std::vector<T> &vector, const T &object) {
  return std::ranges::find(vector, object) != vector.end();
}

inline std::strong_ordering lexCompare(const std::string &left, const std::string &right) {
  if (left == right) {
    return std::strong_ordering::equal;
  }
  return left < right ? std::strong_ordering::less : std::strong_ordering::greater;
}

inline DNF toDNF(const Literal &context, const PartialDNF &partialDNF) {
  DNF result;
  result.reserve(partialDNF.size());
  for (const auto &partialCube : partialDNF) {
    Cube cube;
    cube.reserve(partialCube.size());

    for (const auto &partialLiteral : partialCube) {
      if (std::holds_alternative<Literal>(partialLiteral)) {
        auto l = std::get<Literal>(partialLiteral);
        cube.push_back(std::move(l));
      } else {
        const auto as = std::get<AnnotatedSet>(partialLiteral);
        cube.push_back(context.substituteSet(as));
      }
    }
    result.push_back(cube);
  }
  return result;
}

bool cubeHasNegatedLiteral(const range_of<Literal> auto &cube, const Literal &literal) {
  return std::ranges::any_of(
      cube, [&](const auto &cubeLiteral) { return literal.isNegatedOf(cubeLiteral); });
}

inline bool cubeHasPositiveAtomic(const Cube &cube) {
  return std::ranges::any_of(cube, &Literal::isPositiveAtomic);
}

inline bool isLiteralActive(const Literal &literal, const EventSet &activeEvents) {
  // IMPORTANT includes requires the sets to be sorted
  return std::ranges::includes(activeEvents, literal.normalEvents());
}

inline bool isLiteralActive(const Literal &literal, const SetOfSets &activePairs) {
  // IMPORTANT includes requires the sets to be sorted
  return std::ranges::includes(activePairs, literal.labelBaseCombinations());
}

// activeEvent = event occurs in positive literal
inline EventSet gatherActiveEvents(const Cube &cube) {
  // preconditions:
  assert(validateNormalizedCube(cube));  // cube is normal

  EventSet activeEvents;
  for (const auto &literal : cube) {
    if (literal.negated) {
      continue;
    }

    const auto &literalLabels = literal.events();
    activeEvents.insert(literalLabels.begin(), literalLabels.end());
  }

  return activeEvents;
}

inline void countActiveEvents(const CanonicalSet set,
                              std::vector<std::pair<int, int>> &activeEventCounters) {
  switch (set->operation) {
    case SetOperation::event: {
      auto e = set->label.value();
      auto it = std::ranges::find(activeEventCounters, e, &std::pair<int, int>::first);
      if (it != activeEventCounters.end()) {
        it->second++;
      } else {
        activeEventCounters.emplace_back(e, 1);
      }
      return;
    }
    case SetOperation::image:
    case SetOperation::domain:
      countActiveEvents(set->leftOperand, activeEventCounters);
      return;
    case SetOperation::setIntersection:
    case SetOperation::setUnion:
      countActiveEvents(set->leftOperand, activeEventCounters);
      countActiveEvents(set->rightOperand, activeEventCounters);
      return;
    // TODO (topEvent optimization): case SetOperation::topEvent:
    case SetOperation::baseSet:
    case SetOperation::emptySet:
    case SetOperation::fullSet:
      return;
    default:
      throw std::logic_error("unreachable");
  }
}

// activeEvent = event occurs in positive literal
inline std::optional<int> gatherMinimalOccurringActiveEvent(const Cube &cube) {
  // preconditions:
  assert(validateNormalizedCube(cube));  // cube is normal

  std::vector<std::pair<int, int>> activeEventCounters;
  for (const auto &literal : cube) {
    if (literal.negated || literal.operation != PredicateOperation::setNonEmptiness) {
      continue;
    }

    countActiveEvents(literal.set, activeEventCounters);
  }
  std::ranges::sort(activeEventCounters, std::less<int>{}, &std::pair<int, int>::second);
  return activeEventCounters.empty() ? std::nullopt
                                     : std::optional(activeEventCounters.front().first);
}

// removes all negated literals in cube with events that do not occur in events
// returns removed literals
inline Cube filterNegatedLiterals(Cube &cube, const EventSet &activeEvents) {
  Cube removedLiterals;
  std::erase_if(cube, [&](auto &literal) {
    if (!literal.negated) {
      return false;
    }
    if (!isLiteralActive(literal, activeEvents)) {
      removedLiterals.push_back(literal);
      return true;
    }
    return false;
  });
  return removedLiterals;
}

// TODO: Return value unused
inline void removeUselessLiterals(Cube &cube) {
  const auto &activeEvents = gatherActiveEvents(cube);
  filterNegatedLiterals(cube, activeEvents);
  std::erase_if(cube, [&](const Literal &literal) {
    return literal.negated && literal.operation != PredicateOperation::setNonEmptiness;
  });
}

inline void removeUselessLiterals(DNF &dnf) {
  Stats::diff("removeUselessLiterals").first(flatten(dnf).size());
  for (auto &cube : dnf) {
    removeUselessLiterals(cube);
  }
  Stats::diff("removeUselessLiterals").second(flatten(dnf).size());
}

inline Cube filterNegatedLiterals(Cube &cube, const SetOfSets &activePairs) {
  Cube removedLiterals;
  std::erase_if(cube, [&](auto &literal) {
    if (literal.negated && !isLiteralActive(literal, activePairs)) {
      removedLiterals.push_back(literal);
      return true;
    }
    return false;
  });
  return removedLiterals;
}

// ===================================================================================
// ============================ Benchmarking utility =================================
// ===================================================================================

template <class Unordered>
double measure_unordered_badness(Unordered const &map) {
  auto const lambda = map.size() / static_cast<double>(map.bucket_count());

  auto cost = 0.;
  for (int i = 0; i < map.bucket_count(); i++) cost += map.bucket_size(i) * map.bucket_size(i);
  cost /= map.size();

  return std::max(0., cost / (1 + lambda) - 1);
}

template <class Unordered>
std::pair<int, int> countCollisions(const Unordered &map) {
  int totalCollisions = 0;
  int maxCollisions = 0;
  for (int i = 0; i < map.bucket_count(); i++) {
    int collisions = map.bucket_size(i) - 1;
    if (collisions > 0) {
      totalCollisions += collisions;
      maxCollisions = std::max(maxCollisions, collisions);
    }
  }
  return {totalCollisions, maxCollisions};
}