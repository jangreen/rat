#include <boost/functional/hash.hpp>
#include <iostream>

#include "RegularTableau.h"

// TODO: when hashing all singleton sets are equal, when comparing search for renaming
RegularTableau::Node::Node(std::initializer_list<Literal> cube) : cube(cube) {}
RegularTableau::Node::Node(Cube cube) : cube(std::move(cube)) {}

// hashing and comparison is insensitive to label renaming
bool RegularTableau::Node::operator==(const Node &otherNode) const {
  // shortcuts
  if (cube.size() != otherNode.cube.size()) {
    return false;
  }
  // copy, sort, compare
  Cube c1 = cube;
  Cube c2 = otherNode.cube;
  std::sort(c1.begin(), c1.end());
  std::sort(c2.begin(), c2.end());

  // rename
  rename(c1);
  rename(c2);
  return c1 == c2;
}

size_t std::hash<RegularTableau::Node>::operator()(const RegularTableau::Node &node) const {
  size_t seed = 0;
  Cube copy = node.cube;
  std::sort(copy.begin(), copy.end());
  RegularTableau::rename(copy);
  for (const auto &literal : copy) {
    boost::hash_combine(seed, literal.toString());
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
    auto edgeLabels = childNode->parentNodes[this];

    for (const auto &edgeLabel : edgeLabels) {
      output << "N" << this << " -> " << "N" << childNode << "[";
      if (std::get<0>(edgeLabel).empty()) {
        output << "color=\"grey\", ";
      }
      // } else if (childNode->firstParentNode == this) {
      //   output << "color=\"blue\", ";
      // }
      output << "label =\"";
      for (const auto &edgeValue : std::get<0>(edgeLabel)) {
        output << edgeValue.toString() << ", ";
      }
      output << " | ";
      for (const auto &v : std::get<1>(edgeLabel)) {
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
