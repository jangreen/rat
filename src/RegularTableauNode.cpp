#include <boost/functional/hash.hpp>
#include <iostream>

#include "RegularTableau.h"

// TODO: when hashing all sinlgeton sets are equal, when comparing search for renaming
RegularTableau::Node::Node(std::initializer_list<Formula> formulas) : formulas(formulas) {}
RegularTableau::Node::Node(FormulaSet formulas) : formulas(formulas) {}

bool RegularTableau::Node::operator==(const Node &otherNode) const {
  // shorcuts
  if (formulas.size() != otherNode.formulas.size()) {
    return false;
  }
  // copy, sort, compare
  FormulaSet c1 = formulas;
  FormulaSet c2 = otherNode.formulas;
  std::sort(c1.begin(), c1.end());
  std::sort(c2.begin(), c2.end());
  return c1 == c2;
}

size_t std::hash<RegularTableau::Node>::operator()(const RegularTableau::Node &node) const {
  size_t seed = 0;
  FormulaSet copy = node.formulas;
  std::sort(copy.begin(), copy.end());
  for (const auto &formula : copy) {
    boost::hash_combine(seed, formula.toString());
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
  for (const auto &relation : formulas) {
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
    auto labelString = edgeLabel ? std::get<Formula>(*edgeLabel).toString() : "#";
    std::vector<int> emptyRenaming;
    auto labelRenaming = edgeLabel ? std::get<Renaming>(*edgeLabel) : emptyRenaming;

    output << "N" << this << " -> "
           << "N" << childNode << "[label=\"" << labelString << "\n";
    for (auto label : labelRenaming) {
      output << label << ".";
    }
    output << "\"];" << std::endl;
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
