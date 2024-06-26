#pragma once
#include <fmt/core.h>

#include "Literal.h"
#include "Tableau.h"

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

inline bool validateDNF(const DNF &dnf) {
  return std::ranges::all_of(dnf, [](const auto &cube) { return validateCube(cube); });
}

inline bool validateSet(std::set<Tableau::Node *, Tableau::Node::CompareNodes> s) {
  return std::ranges::all_of(s, [](const auto node) { return node->validate(); });
}

template <typename T>
bool contains(const std::vector<T> &vector, const T &object) {
  return std::ranges::find(vector, object) != vector.end();
}
