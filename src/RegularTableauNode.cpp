#include <iostream>

#include "RegularTableau.h"

RegularTableau::Node::Node(Cube cube) : cube(std::move(cube)) {}

std::pair<RegularTableau::Node *, Renaming> RegularTableau::Node::newNode(Cube cube) {
  // Goal: calculate canonical cube:
  // -> calculate renaming such that two isomorphic cubes C1 and C2 are identical after applying
  // their renaming
  // -> DAG isomorphism (NP-C)
  // note that < (i.e. <=>) cannot be used here due to nondeterminism for setNonEmptiness predicates
  // -> the ordering must be insensitive to event labels!

  // 1) calculate renaming
  // all events occur in positive labels
  Cube positiveCube;
  std::ranges::copy_if(cube, std::back_inserter(positiveCube),
                       [](auto &literal) { return !literal.negated; });
  std::ranges::sort(positiveCube, [](const Literal &first, const Literal &second) {
    if (first.set->toString().size() != second.set->toString().size()) {
      return first.set->toString().size() < second.set->toString().size();
    }
    // TODO: make smart toString comparision
    return first.set->toString() < second.set->toString();
  });
  std::vector<int> labels{};
  for (const auto &literal : positiveCube) {
    for (const auto &l : literal.labels()) {
      if (std::ranges::find(labels, l) == labels.end()) {
        labels.push_back(l);
      }
    }
  }
  Renaming renaming(labels);
  for (auto &literal : cube) {
    literal.rename(renaming);
  }

  // 2) sort cube after unique renaming
  std::ranges::sort(cube);
  auto node = new Node(std::move(cube));
  assert(std::ranges::is_sorted(node->cube));
  return std::pair{node, renaming};
}

bool RegularTableau::Node::validate() const {
  // TODO: cube must be ordered: std::ranges::is_sorted(cube) &&
  // literals must be normal
  return std::ranges::all_of(cube, [](auto &lit) { return lit.isNormal(); });
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

size_t std::hash<RegularTableau::Node>::operator()(
    const RegularTableau::Node &node) const noexcept {
  size_t seed = 0;
  assert(std::ranges::is_sorted(node.cube));
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
  output << std::hash<Node>()(*this);
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
  for (const auto childNode : childNodes) {
    for (const auto &[edges, renaming] : childNode->parentNodes[this]) {
      output << "N" << this << " -> " << "N" << childNode << "[";
      if (edges.empty()) {
        output << "color=\"grey\", ";
      }
      // } else if (childNode->firstParentNode == this) {
      //   output << "color=\"blue\", ";
      // }

      // tooltip = renaming + annotation
      output << "tooltip=\"";
      renaming.toDotFormat(output);
      output << "\", ";

      // label = edges
      output << "label =\"";
      for (const auto &edgeValue : edges) {
        output << edgeValue.toString() << " ";
      }

      output << "\"];" << std::endl;
    }
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
  for (const auto childNode : childNodes) {
    childNode->toDotFormat(output);
  }
}
