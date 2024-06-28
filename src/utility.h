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

inline bool validateNormalizedCube(const Cube &cube) {
  return validateCube(cube) && std::ranges::all_of(cube, [](auto &lit) { return lit.isNormal(); });
}

inline bool validateDNF(const DNF &dnf) {
  return std::ranges::all_of(dnf, [](const auto &cube) { return validateCube(cube); });
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