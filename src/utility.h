#pragma once

#include <iostream>

#include "basic/Literal.h"

inline void print(const DNF &dnf) {
  std::cout << "Cubes:";
  for (auto &cube : dnf) {
    std::cout << "\n";
    for (auto &literal : cube) {
      std::cout << literal.toString() << " , ";
    }
  }
  std::cout << std::endl;
}

inline void print(const Cube &cube) {
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
  // const bool hasDuplicates = std::ranges::adjacent_find(copy) != copy.end();
  // assert(!hasDuplicates);
  return std::ranges::all_of(cube, [](const auto &literal) { return literal.validate(); });
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

inline bool cubeHasNegatedLiteral(const Cube &cube, const Literal &literal) {
  return std::ranges::any_of(
      cube, [&](const auto &cubeLiteral) { return literal.isNegatedOf(cubeLiteral); });
}

inline bool isLiteralActive(const Literal &literal, const EventSet &activeEvents) {
  return std::ranges::includes(activeEvents, literal.normalEvents());
}

inline bool isLiteralActive(const Literal &literal, const SetOfSets &activePairs) {
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
inline Cube filterNegatedLiterals(Cube &cube) {
  const auto &activeEvents = gatherActiveEvents(cube);
  return filterNegatedLiterals(cube, activeEvents);
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
