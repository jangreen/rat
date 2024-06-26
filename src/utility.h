#pragma once
#include "Literal.h"
#include "Tableau.h"

inline bool validateCube(const Cube &cube) {
  return std::ranges::all_of(cube, [](const auto &literal) { return literal.validate(); });
}

inline bool validateDNF(const DNF &dnf) {
  return std::ranges::all_of(dnf, [](const auto &cube) { return validateCube(cube); });
}

inline bool validateNodes(std::set<Tableau::Node *, Tableau::Node::CompareNodes> s) {
  return std::ranges::all_of(s, [](const auto node) { return node->validate(); });
}
