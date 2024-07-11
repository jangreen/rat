#include "RegularNode.h"

#include <iostream>

#include "utility.h"

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
  Cube sortedCube;
  std::ranges::copy_if(cube, std::back_inserter(sortedCube),
                       [](auto &literal) { return !literal.negated || literal.hasTopEvent(); });
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
    for (const auto &l : literal.topEvents()) {
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
  Renaming renaming(events);
  for (auto &literal : cube) {
    literal.rename(renaming);
  }

  // 2) sort cube after unique renaming
  std::ranges::sort(cube);
  auto *node = new RegularNode(std::move(cube));
  assert(std::ranges::is_sorted(node->cube));
  assert(node->validate());
  return std::pair{node, renaming};
}

bool RegularNode::validate() const {
  // TODO: cube must be ordered: std::ranges::is_sorted(cube) &&
  // cube s valid and has no duplicates
  assert(validateCube(cube));
  // literals must be normal
  const bool literalsAreNormal =
      std::ranges::all_of(cube, [](auto &lit) { return lit.isNormal(); });
  assert(literalsAreNormal);
  return literalsAreNormal;
}

size_t RegularNode::Hash::operator()(const std::unique_ptr<RegularNode> &node) const {
  return std::hash<RegularNode>()(*node);
}

// FIXME calculate cached lazy property
// hashing and comparison is insensitive to label renaming
bool RegularNode::operator==(const RegularNode &otherNode) const {
  // shortcuts
  if (cube.size() != otherNode.cube.size()) {
    return false;
  }
  return cube == otherNode.cube;
}

bool RegularNode::Equal::operator()(const std::unique_ptr<RegularNode> &node1,
                                    const std::unique_ptr<RegularNode> &node2) const {
  return *node1 == *node2;
}

void RegularNode::toDotFormat(std::ofstream &output) {
  if (printed) {
    return;
  }

  output << "N" << this << "[tooltip=\"";
  // debug
  output << this << "\n\n";
  output << "Hash:\n" << std::hash<RegularNode>()(*this);
  // label/cube
  output << "\", label=\"";
  for (const auto &lit : cube) {
    output << lit.toString() << "\n";
  }
  output << "\"";
  // color closed branches
  if (closed) {
    output << ", color=green";
  }
  output << "];" << std::endl;
  // edges
  for (const auto epsilonChild : epsilonChildren) {
    output << "N" << this << " -> " << "N" << epsilonChild;
    output << "[" << "color=\"grey" << "\"];\n";
  }

  for (const auto child : children) {
    const auto label = child->parents.at(this);
    output << "N" << this << " -> " << "N" << child;
    output << "[tooltip=\"";
    label.toDotFormat(output);
    output << "\"];\n";
  }
  /*/ parents
  for (const auto parentNode : parentNodes) {
    output << "N" << this << " -> "
           << "N" << parentNode.first << "[color=\"grey\"];" << std::endl;
  }  //*/
  // root parents
  // for (const auto parentNode : rootParents) {
  //   output << "N" << this << " -> " << "N" << parentNode << "[color=\"brown\"];" <<
  //   std::endl;
  // }
  printed = true;

  // children
  for (const auto child : children) {
    child->toDotFormat(output);
  }
  for (const auto child : epsilonChildren) {
    child->toDotFormat(output);
  }
}
