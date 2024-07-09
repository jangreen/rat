#pragma once

#include <iostream>

#include "Literal.h"

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
    std::cout << literal.toString() << " , ";
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

template <typename T>
bool push_back_unique(std::vector<T> &vector, const T &object) {
  const bool add = !contains(vector, object);
  if (add) {
    vector.push_back(object);
  }
  return add;
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

inline bool isLiteralActive(const Literal &literal, const EventSet &activeLabels) {
  return std::ranges::includes(activeLabels, literal.events());
}

inline bool isLiteralActive(const Literal &literal, const SetOfSets &combinations) {
  return std::ranges::includes(combinations, literal.labelBaseCombinations());
}

// removes all negated literals in cube with events that do not occur in events
// returns removed literals
inline Cube filterNegatedLiterals(Cube &cube, const EventSet events) {
  Cube removedLiterals;
  std::erase_if(cube, [&](auto &literal) {
    if (!literal.negated) {
      return false;
    }
    if (!isLiteralActive(literal, events)) {
      removedLiterals.push_back(literal);
      return true;
    }
    return false;
  });
  return removedLiterals;
}

// removes all negated literals in cube with event/base relation combination that do not occur
// positive
// returns removed literals
inline void filterNegatedLiterals(Cube &cube, const SetOfSets &combinations,
                                  Cube &removedLiterals) {
  std::erase_if(cube, [&](auto &literal) {
    if (!literal.negated) {
      return false;
    }
    if (!isLiteralActive(literal, combinations)) {
      removedLiterals.push_back(literal);
      return true;
    }
    return false;
  });
}