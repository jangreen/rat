#pragma once
#include "Literal.h"
#include "Tableau.h"

inline bool validateCube(const Cube &cube) {
  return std::ranges::all_of(cube, [](const auto &literal) { return literal.validate(); });
}

inline bool validateDNF(const DNF &dnf) {
  return std::ranges::all_of(dnf, [](const auto &cube) { return validateCube(cube); });
}

inline bool validateUnreducedNodes(Tableau *tableau) {
  return std::ranges::all_of(tableau->unreducedNodes.__get_container(),
                             [](const auto unreducedNode) { return unreducedNode->validate(); });
}
