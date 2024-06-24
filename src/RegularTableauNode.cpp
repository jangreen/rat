#include <iostream>

#include "RegularTableau.h"

// TODO: when hashing all singleton sets are equal, when comparing search for renaming
RegularTableau::Node::Node(Cube cube) {
  // order cube
  std::sort(cube.begin(), cube.end());

  // calculate renaming
  Renaming renaming{};
  for (auto &literal : cube) {
    for (const auto &l : literal.labels()) {
      if (std::ranges::find(renaming, l) == renaming.end()) {
        renaming.push_back(l);
      }
    }
  }
  this->renaming = renaming;

  // rename
  for (auto &literal : cube) {
    literal.rename(renaming, false);
  }
  this->cube = std::move(cube);
}

// FIXME calculate cached lazy property
// hashing and comparison is insensitive to label renaming
bool RegularTableau::Node::operator==(const Node &otherNode) const {
  // shortcuts
  if (cube.size() != otherNode.cube.size()) {
    return false;
  }
  return cube == otherNode.cube;
}

size_t std::hash<RegularTableau::Node>::operator()(const RegularTableau::Node &node) const noexcept {
  size_t seed = 0;
  for (const auto &literal : node.cube) {
    boost::hash_combine(seed, std::hash<Literal>()(literal));
  }
  return seed;
}

size_t RegularTableau::Node::Hash::operator()(const std::unique_ptr<Node> &node) const {
  return std::hash<Node>()(*node);
}

bool RegularTableau::Node::Equal::operator()(const std::unique_ptr<Node> &node1,
                                             const std::unique_ptr<Node> &node2) const {
  return *node1 == *node2;
}

void RegularTableau::Node::toDotFormat(std::ofstream &output) {
  if (printed) {
    return;
  }

  output << "N" << this << "[tooltip=\"";
  // debug
  output << std::hash<Node>()(*this) << std::endl << std::endl;
  // label/cube
  output << "\", label=\"";
  for (const auto &relation : cube) {
    output << relation.toString() << std::endl;
  }
  output << "\"";
  // color closed branches
  if (closed) {
    output << ", color=green";
  }
  output << "];" << std::endl;
  // edges
  for (const auto childNode : childNodes) {
    for (const auto &[edges, labels] : childNode->parentNodes[this]) {
      output << "N" << this << " -> " << "N" << childNode << "[";
      if (edges.empty()) {
        output << "color=\"grey\", ";
      }
      // } else if (childNode->firstParentNode == this) {
      //   output << "color=\"blue\", ";
      // }
      output << "label =\"";
      for (const auto &edgeValue : edges) {
        output << edgeValue.toString() << ", ";
      }
      output << " | ";
      for (const auto &v : labels) {
        output << v << ", ";
      }
      output << "\n\"];" << std::endl;
    }
  }
  /*/ parents
  for (const auto parentNode : parentNodes) {
    output << "N" << this << " -> "
           << "N" << parentNode.first << "[color=\"grey\"];" << std::endl;
  }  //*/
  /*/ root parents
  for (const auto parentNode : rootParents) {
    output << "N" << this << " -> "
           << "N" << parentNode << "[color=\"brown\"];" << std::endl;
  }  //*/
  printed = true;

  // children
  for (const auto childNode : childNodes) {
    childNode->toDotFormat(output);
  }
}
