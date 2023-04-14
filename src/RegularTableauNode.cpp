#include <boost/functional/hash.hpp>

#include "RegularTableau.h"

RegularTableau::Node::Node(std::initializer_list<Relation> relations) : relations(relations) {}
RegularTableau::Node::Node(Clause relations) : relations(relations) {}

bool RegularTableau::Node::operator==(const Node &otherNode) const {
  // shorcuts
  if (relations.size() != otherNode.relations.size()) {
    return false;
  }
  // copy, sort, compare
  Clause c1 = relations;
  Clause c2 = otherNode.relations;
  std::sort(c1.begin(), c1.end());
  std::sort(c2.begin(), c2.end());
  return c1 == c2;
}

size_t std::hash<RegularTableau::Node>::operator()(const RegularTableau::Node &node) const {
  size_t seed = 0;
  Clause copy = node.relations;
  sort(copy.begin(), copy.end());
  for (const auto &relation : copy) {
    boost::hash_combine(seed, relation.toString());
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

  output << "N" << this << "[label=\"";
  for (const auto &relation : relations) {
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
    auto edgeLabel = childNode->parentNodes[this];
    auto labelString = edgeLabel ? std::get<Metastatement>(*edgeLabel).toString() : "#";
    output << "N" << this << " -> "
           << "N" << childNode << "[label=\"" << labelString << "\"];" << std::endl;
  }
  // parents
  for (const auto parentNode : parentNodes) {
    output << "N" << this << " -> "
           << "N" << parentNode.first << "[color=\"grey\"];" << std::endl;
  }
  // root parents
  for (const auto parentNode : rootParents) {
    output << "N" << this << " -> "
           << "N" << parentNode << "[color=\"brown\"];" << std::endl;
  }
  printed = true;

  // children
  for (const auto childNode : childNodes) {
    childNode->toDotFormat(output);
  }
}