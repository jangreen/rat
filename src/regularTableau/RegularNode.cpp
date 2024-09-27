#include "RegularNode.h"

#include <iostream>
#include <ranges>

#include "../utility.h"

RegularNode::RegularNode(Cube cube) : cube(std::move(cube)) {}

std::pair<RegularNode *, Renaming> RegularNode::newNode(Cube cube) {
  // Goal: calculate canonical cube:
  // -> calculate renaming such that two isomorphic cubes C1 and C2 are identical after applying
  // their renaming
  // -> DAG isomorphism (NP-C)
  // note that < (i.e. <=>) cannot be used here due to nondeterminism for setNonEmptiness predicates
  // -> the ordering must be insensitive to events!

  // 1) calculate renaming
  // all (existential) events occur in positive literal
  // all (universal) events (aka topEvents) occur in negated literals
  assert(validateNormalizedCube(cube));
  Cube sortedCube;
  std::ranges::copy_if(cube, std::back_inserter(sortedCube),
                       [](auto &literal) { return !literal.negated /*|| literal.hasTopEvent()*/; });
  std::ranges::sort(sortedCube, [](const Literal &first, const Literal &second) {
    if (first.negated != second.negated) {
      return first.negated < second.negated;
    }
    if (first.toString().size() != second.toString().size()) {
      return first.toString().size() < second.toString().size();
    }
    // TODO: make smart toString comparision
    return first.toString() < second.toString();
  });
  std::vector<int> events{};
  for (const auto &literal : sortedCube) {
    for (const auto &l : literal.events()) {
      if (std::ranges::find(events, l) == events.end()) {
        events.push_back(l);
      }
    }
  }
  assert(validateNormalizedCube(cube));
  assert(std::ranges::all_of(cube, [&](const auto &literal) {
    assert(std::ranges::all_of(literal.events(),
                               [&](const auto event) { return contains(events, event); }));
    return true;
  }));
  Renaming renaming = Renaming::minimal(events);
  renameCube(renaming, cube);

#if (DEBUG)
  // validate: all events must be continuous integer interval from 0
  EventSet allEvents;
  for (const auto &literal : cube) {
    const auto &literalEvents = literal.events();
    allEvents.insert(literalEvents.begin(), literalEvents.end());
  }
  if (!allEvents.empty()) {
    const auto maxEvent = std::ranges::max(allEvents);
    assert(maxEvent == allEvents.size() - 1);
  }
  assert(allEvents.size() == events.size());
#endif

  // 2) sort cube after unique renaming
  std::ranges::sort(cube);
  auto *node = new RegularNode(std::move(cube));
  assert(std::ranges::is_sorted(node->cube));
  assert(node->validate());
  return std::pair{node, renaming};
}

bool RegularNode::validate() const {
  // cube is valid and has no duplicates
  assert(validateCube(cube));
  // literals must be normal
  const bool literalsAreNormal = std::ranges::all_of(cube, &Literal::isNormal);
  assert(literalsAreNormal);
  // edges are valid
  assert(std::ranges::all_of(children, [&](const RegularNode *child) {
    return child->parents.contains(const_cast<RegularNode *>(this));
  }));
  assert(std::ranges::all_of(epsilonChildren, [&](const RegularNode *epsilonChild) {
    return epsilonChild->epsilonParents.contains(const_cast<RegularNode *>(this));
  }));
  return literalsAreNormal;
}

size_t RegularNode::Hash::operator()(const std::unique_ptr<RegularNode> &node) const {
  return std::hash<RegularNode>()(*node);
}

void RegularNode::toDotFormat(std::ofstream &output) const {
  output << "N" << this << "[tooltip=\"";
  output << this << "\n\n";

  // label is cube
  output << "\", label=\"";
  for (const auto &lit : cube) {
    output << lit.toString() << "\n";
  }
  output << "\"";

  // color green if closed
  if (closed) {
    output << ", color=green, fontcolor=green";
  }
  output << "];\n";
}
