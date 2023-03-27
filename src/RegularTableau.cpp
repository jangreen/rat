#include "RegularTableau.h"

#include <algorithm>
#include <iostream>
#include <tuple>
#include <utility>

#include "Tableau.h"

// hashing
// using boost::hash_combine
template <class T>
inline void hash_combine(std::size_t &seed, T const &v) {
  seed ^= std::hash<T>()(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

size_t std::hash<RegularTableau::Node>::operator()(const RegularTableau::Node &node) const {
  size_t seed = 0;
  Clause copy = node.relations;
  sort(copy.begin(), copy.end());
  for (const auto &relation : copy) {
    hash_combine(seed, relation.toString());  // TODO: use std::string reprstation
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

RegularTableau::Node::Node(std::initializer_list<Relation> relations) : relations(relations) {}
RegularTableau::Node::Node(Clause relations) : relations(relations) {}

void RegularTableau::Node::toDotFormat(std::ofstream &output) {
  output << "N" << this << "[label=\"";
  // TODO: remove output << std::hash<Node>()(*this) << std::endl;
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
  for (auto &[childNode, edgeLabel] : childNodes) {
    output << "N" << this << " -> "
           << "N" << childNode << ";" << std::endl;
  }
  printed = true;

  // children
  for (auto &[childNode, edgeLabel] : childNodes) {
    if (!childNode->printed) {
      childNode->toDotFormat(output);
    }
  }
}

RegularTableau::RegularTableau(std::initializer_list<Relation> initalRelations)
    : RegularTableau(std::vector(initalRelations)) {}
RegularTableau::RegularTableau(Clause initalRelations) {
  auto dnf = DNF(initalRelations);
  for (auto clause : dnf) {
    addNode(nullptr, clause);
  }
  // TODO: remove exportProof("initalized");
}

std::vector<Assumption> RegularTableau::assumptions;

std::vector<Clause> RegularTableau::DNF(const Clause &clause) {
  Tableau tableau{clause};
  return tableau.DNF();
}

// helper
std::string calcExpandedRelation(const Tableau &tableau) {
  Tableau::Node *node = &(*tableau.rootNode);
  while (node != nullptr) {  // exploit that only alpha rules applied
    if (node->metastatement) {
      return *node->metastatement->baseRelation;
    }
    node = &(*node->leftNode);
  }
}

// node has only normal terms
bool RegularTableau::expandNode(Node *node) {
  Tableau tableau{node->relations};
  // TODO: remove  tableau.exportProof("dnfcalc");  // initial
  bool expandable = tableau.applyModalRule();
  if (expandable) {
    // TODO: remove  tableau.exportProof("dnfcalc");  // modal
    auto baseRelation = calcExpandedRelation(tableau);
    auto request = tableau.calcReuqest();
    // TODO: remove tableau.exportProof("dnfcalc");  // request
    if (tableau.rootNode->isClosed()) {
      // can happen with empty hypotheses
      node->closed = true;
      return true;
    }
    auto dnf = DNF(request);
    for (const auto &clause : dnf) {
      addNode(node, clause, baseRelation);
    }
    if (dnf.empty()) {
      node->closed = true;
    }
    return true;
  }
  return false;
}

// clause is in normal form
// parent == nullptr -> rootNode
void RegularTableau::addNode(Node *parent, Clause clause, std::string expandedBaseRelation) {
  // calculate renaming & rename
  std::vector<int> renaming;
  for (const auto &literal : clause) {
    if (!literal.negated) {
      renaming = literal.calculateRenaming();
      break;
    }
  }

  for (Relation &literal : clause) {
    literal.rename(renaming);
  }

  // saturation phase
  saturate(clause);
  auto saturatedDNF = DNF(clause);
  for (auto saturatedClause : saturatedDNF) {
    // create node, edges, push to unreduced nodes
    auto newNode = std::make_unique<Node>(saturatedClause);
    newNode->parentNodeBaseRelation = expandedBaseRelation;
    newNode->parentNode = &(*parent);

    auto existingNode = nodes.find(newNode);
    if (existingNode != nodes.end()) {
      // equivalent node exists
      if (parent != nullptr) {
        std::tuple<Node *, std::vector<int>> edge{existingNode->get(), renaming};
        parent->childNodes.push_back(edge);
      }
    } else {
      // equivalent node does not exist
      if (parent != nullptr) {
        std::tuple<Node *, std::vector<int>> edge{newNode.get(), renaming};
        parent->childNodes.push_back(edge);
      } else {
        // new root node
        rootNodes.push_back(newNode.get());
      }
      unreducedNodes.push(newNode.get());
      nodes.emplace(std::move(newNode));
    }
  }
}

std::optional<Relation> RegularTableau::saturateRelation(const Relation &relation) {
  if (relation.saturated || relation.saturatedId) {
    return std::nullopt;
  }

  std::optional<Relation> leftSaturated;
  std::optional<Relation> rightSaturated;

  switch (relation.operation) {
    case Operation::none:
    case Operation::identity:
    case Operation::empty:
      return std::nullopt;
    case Operation::base:
      for (const auto &assumption : assumptions) {
        // TODO fast get regular assumption
        if (assumption.type == AssumptionType::regular &&
            *assumption.baseRelation == *relation.identifier) {
          Relation leftSide(assumption.relation);
          leftSide.label = relation.label;
          return leftSide;
        }
      }
      return std::nullopt;
    case Operation::choice:
    case Operation::intersection:
      leftSaturated = saturateRelation(*relation.leftOperand);
      rightSaturated = saturateRelation(*relation.rightOperand);
      break;
    case Operation::composition:
    case Operation::converse:
    case Operation::transitiveClosure:
      leftSaturated = saturateRelation(*relation.leftOperand);
      break;
    default:
      break;
  }
  if (!leftSaturated && !rightSaturated) {
    return std::nullopt;
  }
  if (!leftSaturated) {
    leftSaturated = Relation(*relation.leftOperand);
  }
  if (!rightSaturated && relation.rightOperand != nullptr) {
    rightSaturated = Relation(*relation.rightOperand);
  }
  if (rightSaturated) {
    Relation saturated(relation.operation, std::move(*leftSaturated), std::move(*rightSaturated));
    return saturated;
  }
  Relation saturated(relation.operation, std::move(*leftSaturated));
  return saturated;
}

std::optional<Relation> RegularTableau::saturateIdRelation(const Assumption &assumption,
                                                           const Relation &relation) {
  if (relation.saturatedId || relation.operation == Operation::identity ||
      relation.operation == Operation::empty) {
    return std::nullopt;
  }
  if (relation.label) {
    Relation copy(relation);
    copy.label = std::nullopt;
    copy.negated = false;
    Relation assumptionR(assumption.relation);
    assumptionR.label = relation.label;
    Relation r(Operation::composition, std::move(assumptionR), std::move(copy));
    return r;
  }

  std::optional<Relation> leftSaturated;
  std::optional<Relation> rightSaturated;

  switch (relation.operation) {
    case Operation::none:
    case Operation::identity:
    case Operation::empty:
    case Operation::base:
      return std::nullopt;
    case Operation::choice:
    case Operation::intersection:
      leftSaturated = saturateIdRelation(assumption, *relation.leftOperand);
      rightSaturated = saturateIdRelation(assumption, *relation.rightOperand);
      break;
    case Operation::composition:
    case Operation::converse:
    case Operation::transitiveClosure:
      leftSaturated = saturateIdRelation(assumption, *relation.leftOperand);
      break;
    default:
      break;
  }

  // TODO_ support intersections -> different identity or no saturations
  if (!leftSaturated && !rightSaturated) {
    return std::nullopt;
  }
  if (!leftSaturated) {
    leftSaturated = Relation(*relation.leftOperand);
  }
  if (!rightSaturated) {
    rightSaturated = Relation(*relation.rightOperand);
  }
  Relation saturated(relation.operation, std::move(*leftSaturated), std::move(*rightSaturated));
  return saturated;
}

void RegularTableau::saturate(Clause &clause) {
  // return; // TODO remove:
  Clause saturatedRelations;
  // regular assumptions
  for (const Relation &literal : clause) {
    if (literal.negated) {
      std::optional<Relation> saturated = saturateRelation(literal);
      if (saturated) {
        saturated->negated = true;
        saturatedRelations.push_back(std::move(*saturated));
      }
    }
  }

  for (const auto &assumption : assumptions) {
    std::vector<int> nodeLabels;
    switch (assumption.type) {
      case AssumptionType::empty:
        for (const auto &literal : clause) {
          for (auto label : literal.labels()) {
            if (find(nodeLabels.begin(), nodeLabels.end(), label) == nodeLabels.end()) {
              nodeLabels.push_back(label);
            }
          }
        }

        for (auto label : nodeLabels) {
          Relation leftSide(assumption.relation);
          Relation full(Operation::full);
          Relation r(Operation::composition, std::move(leftSide), std::move(full));
          r.saturated = true;
          r.label = label;
          r.negated = true;
          saturatedRelations.push_back(std::move(r));
        }
        break;
      case AssumptionType::identity:
        for (const auto &literal : clause) {
          if (literal.negated) {
            auto saturated = saturateIdRelation(assumption, literal);
            if (saturated) {
              saturated->negated = true;
              saturatedRelations.push_back(*saturated);
            }
          }
        }
      default:
        break;
    }
  }

  clause.insert(clause.end(), saturatedRelations.begin(), saturatedRelations.end());
  // remove duplicates since it has not been check when inserting
  sort(clause.begin(), clause.end());
  clause.erase(unique(clause.begin(), clause.end()), clause.end());
}

bool RegularTableau::solve() {
  while (!unreducedNodes.empty()) {
    auto currentNode = unreducedNodes.top();
    unreducedNodes.pop();
    /* TODO: remove: */
    exportProof("regular");
    if (!expandNode(currentNode)) {
      extractCounterexample(&(*currentNode));
      std::cout << "False." << std::endl;
      return false;
    }
  }
  std::cout << "True." << std::endl;
  return true;
}

void RegularTableau::extractCounterexample(Node *openNode) {
  // TODO: work in progress
  std::cout << "Counterexample:";
  Node *node = openNode;
  while (node != nullptr) {
    std::cout << " " << node->parentNodeBaseRelation;
    node = node->parentNode;
  }
  std::cout << std::endl;
}

void RegularTableau::toDotFormat(std::ofstream &output) const {
  for (const auto &node : nodes) {  // reset printed property
    node->printed = false;
  }
  output << "digraph {" << std::endl << "node[shape=\"box\"]" << std::endl;
  for (auto rootNode : rootNodes) {
    rootNode->toDotFormat(output);
    output << "root -> "
           << "N" << rootNode << ";" << std::endl;
  }
  output << "}" << std::endl;
}

void RegularTableau::exportProof(std::string filename) const {
  std::ofstream file(filename + ".dot");
  toDotFormat(file);
  file.close();
}

bool RegularTableau::Node::operator==(const Node &otherNode) const {
  // shorcuts
  if (relations.size() != otherNode.relations.size()) {
    return false;
  }
  // copy, sort, compare O(n log n)
  Clause c1 = relations;
  Clause c2 = otherNode.relations;
  std::sort(c1.begin(), c1.end());
  std::sort(c2.begin(), c2.end());
  return c1 == c2;
}
