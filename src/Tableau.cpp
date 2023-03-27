#include "Tableau.h"

#include <algorithm>
#include <iostream>
#include <tuple>
#include <utility>

#include "Metastatement.h"
#include "ProofRule.h"

Tableau::Node::Node(Tableau *tableau, const Relation &&relation)
    : tableau(tableau), relation(relation) {}
Tableau::Node::Node(Tableau *tableau, const Metastatement &&metastatement)
    : tableau(tableau), metastatement(metastatement) {}

bool Tableau::Node::isClosed() {
  // Lazy evaluated closed nodes
  bool expanded = leftNode != nullptr;
  closed = closed ||
           (expanded && leftNode->isClosed() && (rightNode == nullptr || rightNode->isClosed()));
  return closed;
}

bool Tableau::Node::isLeaf() const { return (leftNode == nullptr) && (rightNode == nullptr); }

// helper
bool checkIfClosed(Tableau::Node *node, const Relation &relation) {
  if (relation.negated && relation.operation == Operation::full) {
    return true;
  }

  while (node != nullptr) {
    if (node->relation && *node->relation == relation &&
        node->relation->negated != relation.negated) {
      return true;
    }
    node = node->parentNode;
  }
  return false;
}
bool checkIfDuplicate(Tableau::Node *node, const Relation &relation) {
  while (node != nullptr) {
    if (node->relation && *node->relation == relation) {
      return true;
    }
    node = node->parentNode;
  }
  return false;
}

// helper
std::optional<Tableau::Node> createNode(Tableau::Node *parent, const Relation &relation) {
  // create new node by copying
  Tableau::Node newNode(parent->tableau, Relation(relation));
  newNode.closed = checkIfClosed(parent, relation);
  if (!newNode.closed && checkIfDuplicate(parent, relation)) {
    return std::nullopt;
  }
  newNode.parentNode = parent;
  newNode.parentMetastatement = (parent->metastatement) ? parent : parent->parentMetastatement;
  return newNode;
}

void Tableau::Node::appendBranches(const Relation &leftRelation) {
  if (!isClosed() && isLeaf()) {
    auto newNode = createNode(this, leftRelation);
    if (newNode) {
      leftNode = std::make_unique<Node>(std::move(*newNode));
      if (!leftNode->closed) {
        tableau->unreducedNodes.push(leftNode.get());
      }
    }
  } else {
    if (leftNode != nullptr) {
      leftNode->appendBranches(leftRelation);
    }
    if (rightNode != nullptr) {
      rightNode->appendBranches(leftRelation);
    }
  }
}

void Tableau::Node::appendBranches(const Relation &leftRelation, const Relation &rightRelation) {
  if (!isClosed() && isLeaf()) {
    auto newNodeLeft = createNode(this, leftRelation);
    auto newNodeRight = createNode(this, rightRelation);
    if (newNodeLeft) {
      leftNode = std::make_unique<Node>(std::move(*newNodeLeft));
      if (!leftNode->closed) {
        tableau->unreducedNodes.push(leftNode.get());
      }
    }
    if (newNodeRight) {
      rightNode = std::make_unique<Node>(std::move(*newNodeRight));
      if (!rightNode->closed) {
        tableau->unreducedNodes.push(rightNode.get());
      }
    }
  } else {
    if (leftNode != nullptr) {
      leftNode->appendBranches(leftRelation, rightRelation);
    }
    if (rightNode != nullptr) {
      rightNode->appendBranches(leftRelation, rightRelation);
    }
  }
}
// TODO DRY
void Tableau::Node::appendBranches(const Metastatement &metastatement) {
  if (!isClosed() && isLeaf()) {
    leftNode = std::make_unique<Node>(tableau, Metastatement(metastatement));
    leftNode->parentNode = this;
    leftNode->parentMetastatement = (this->metastatement) ? this : parentMetastatement;
    tableau->unreducedNodes.push(leftNode.get());
  } else {
    if (leftNode != nullptr) {
      leftNode->appendBranches(metastatement);
    }
    if (rightNode != nullptr) {
      rightNode->appendBranches(metastatement);
    }
  }
}

void Tableau::Node::toDotFormat(std::ofstream &output) const {
  output << "N" << this << "[label=\"";
  if (relation) {
    output << relation->toString();
  } else {
    output << metastatement->toString();
  }
  output << "\"";
  // color closed branches
  if (closed) {
    output << ", fontcolor=green";
  }
  output << "];" << std::endl;
  // children
  if (leftNode != nullptr) {
    leftNode->toDotFormat(output);
    output << "N" << this << " -- "
           << "N" << leftNode << ";" << std::endl;
  }
  if (rightNode != nullptr) {
    rightNode->toDotFormat(output);
    output << "N" << this << " -- "
           << "N" << rightNode << ";" << std::endl;
  }
}

template <>
bool Tableau::Node::applyRule<ProofRule::propagation>() {
  bool appliedRule = false;
  if (metastatement && metastatement->type == MetastatementType::labelEquality) {
    Tableau::Node *currentNode = parentNode;
    while (currentNode != nullptr) {
      if (currentNode->relation && currentNode->relation->negated) {
        auto rProp = currentNode->relation->applyRuleRecursive<ProofRule::propagation, Relation>(
            &(*metastatement));
        if (rProp) {
          appliedRule = true;
          rProp->negated = currentNode->relation->negated;
          appendBranches(*rProp);
        }
      }
      currentNode = currentNode->parentNode;
    }
  }
  return appliedRule;
}

template <>
bool Tableau::Node::applyRule<ProofRule::negA>() {
  bool appliedRule = false;
  if (metastatement && metastatement->type == MetastatementType::labelRelation) {
    Tableau::Node *currentNode = parentNode;
    while (currentNode != nullptr) {
      if (currentNode->relation && currentNode->relation->negated) {
        auto rNegA =
            currentNode->relation->applyRuleRecursive<ProofRule::negA, Relation>(&(*metastatement));
        if (rNegA) {
          appliedRule = true;
          rNegA->negated = currentNode->relation->negated;
          appendBranches(*rNegA);
        }
      }
      currentNode = currentNode->parentNode;
    }
  } else {
    Tableau::Node *currentMetaNode = parentMetastatement;
    while (currentMetaNode != nullptr) {
      auto rNegA = relation->applyRuleRecursive<ProofRule::negA, Relation>(
          &(*currentMetaNode->metastatement));
      if (rNegA) {
        appliedRule = true;
        rNegA->negated = relation->negated;
        appendBranches(*rNegA);
      }
      currentMetaNode = currentMetaNode->parentMetastatement;
    }
  }
  return appliedRule;
}

bool Tableau::Node::CompareNodes::operator()(const Node *left, const Node *right) const {
  if (left->relation && right->relation) {
    return left->relation->negated < right->relation->negated;
  } else if (right->relation) {
    return (!!left->metastatement) < right->relation->negated;
  }
  return true;
}

Tableau::Tableau(std::initializer_list<Relation> initalRelations)
    : Tableau(std::vector(initalRelations)) {}
Tableau::Tableau(Clause initalRelations) {
  Node *currentNode = nullptr;
  for (auto relation : initalRelations) {
    auto newNode = std::make_unique<Node>(this, Relation(relation));
    newNode->parentNode = currentNode;
    Node *temp = newNode.get();
    if (rootNode == nullptr) {
      rootNode = std::move(newNode);
    } else if (currentNode != nullptr) {
      currentNode->leftNode = std::move(newNode);
    }

    currentNode = temp;
    unreducedNodes.push(temp);
  }
}

// helper
bool applyDNFRule(Tableau::Node *node) {
  auto rId = node->relation->applyRuleRecursive<ProofRule::id, Relation>();
  if (rId) {
    rId->negated = node->relation->negated;
    node->appendBranches(*rId);
    return true;
  }
  auto rComposition = node->relation->applyRuleRecursive<ProofRule::composition, Relation>();
  if (rComposition) {
    rComposition->negated = node->relation->negated;
    node->appendBranches(*rComposition);
    return true;
  }
  auto rIntersection = node->relation->applyRuleRecursive<ProofRule::intersection, Relation>();
  if (rIntersection) {
    rIntersection->negated = node->relation->negated;
    node->appendBranches(*rIntersection);
    return true;
  }
  if (!node->relation->negated) {
    auto rAt =
        node->relation->applyRuleRecursive<ProofRule::at, std::tuple<Relation, Metastatement>>();
    if (rAt) {
      auto &[r1, metastatement] = *rAt;
      r1.negated = node->relation->negated;
      node->appendBranches(r1);
      node->appendBranches(metastatement);
      return true;
    }
  } else {
    auto rNegAt = node->relation->applyRuleRecursive<ProofRule::negAt, Relation>();
    if (rNegAt) {
      auto r1 = *rNegAt;
      r1.negated = node->relation->negated;
      node->appendBranches(r1);
      return true;
    }
  }
  auto rChoice =
      node->relation->applyRuleRecursive<ProofRule::choice, std::tuple<Relation, Relation>>();
  if (rChoice) {
    auto &[r1, r2] = *rChoice;
    r1.negated = node->relation->negated;
    r2.negated = node->relation->negated;
    if (node->relation->negated) {
      node->appendBranches(r1);
      node->appendBranches(r2);
    } else {
      node->appendBranches(r1, r2);
    }
    return true;
  }

  auto rTransitiveClosure =
      node->relation
          ->applyRuleRecursive<ProofRule::transitiveClosure, std::tuple<Relation, Relation>>();
  if (rTransitiveClosure) {
    auto &[r1, r2] = *rTransitiveClosure;
    r1.negated = node->relation->negated;
    r2.negated = node->relation->negated;
    if (node->relation->negated) {
      node->appendBranches(r1);
      node->appendBranches(r2);
    } else {
      node->appendBranches(r1, r2);
    }
    return true;
  }
  return false;
}

bool Tableau::applyRule(Node *node) {
  if (node->metastatement) {
    if (node->metastatement->type == MetastatementType::labelRelation) {
      node->applyRule<ProofRule::negA>();
      // no rule applicable anymore (metastatement)
      return false;  // TODO: this value may be true
    } else {
      node->applyRule<ProofRule::propagation>();
      return true;
    }
  } else if (node->relation) {
    if (node->relation->isNormal()) {
      if (node->relation->negated) {
        if (node->applyRule<ProofRule::negA>()) {
          return true;
        }
      } else {
        auto rA =
            node->relation->applyRuleRecursive<ProofRule::a, std::tuple<Relation, Metastatement>>();
        if (rA) {
          auto &[r1, metastatement] = *rA;
          r1.negated = node->relation->negated;
          node->appendBranches(r1);
          node->appendBranches(metastatement);
          return true;
        }
      }
    } else if (applyDNFRule(node)) {
      return true;
    }
  }

  // "no rule applicable"
  /* TODO
  case Rule::empty:
  case Rule::propagation:
  case Rule::at:
  case Rule::negAt:
  case Rule::converseA:
  case Rule::negConverseA:*/
  return false;
}

bool Tableau::solve(int bound) {
  while (!unreducedNodes.empty() && bound > 0) {
    bound--;
    auto currentNode = unreducedNodes.top();
    unreducedNodes.pop();
    exportProof("infinite");
    applyRule(currentNode);
  }
}

// helper
std::vector<Clause> extractDNF(Tableau::Node *node) {
  if (node == nullptr || node->isClosed()) {
    return {};
  }
  if (node->isLeaf()) {
    if (node->relation && node->relation->isNormal()) {
      return {{*node->relation}};
    }
    return {};
  } else {
    std::vector<Clause> left = extractDNF(node->leftNode.get());
    std::vector<Clause> right = extractDNF(node->rightNode.get());
    left.insert(left.end(), right.begin(), right.end());
    if (node->relation && node->relation->isNormal()) {
      if (left.empty()) {
        Clause newClause({*node->relation});
        left.push_back(std::move(newClause));
      } else {
        for (Clause &clause : left) {
          clause.push_back(*node->relation);
        }
      }
    }
    return left;
  }
}

std::vector<Clause> Tableau::DNF() {
  while (!unreducedNodes.empty()) {
    auto currentNode = unreducedNodes.top();
    unreducedNodes.pop();
    // TODO: remove exportProof("dnfcalc");
    if (currentNode->metastatement) {
      // only equality meatstatement possible
      currentNode->applyRule<ProofRule::propagation>();
    } else {
      applyDNFRule(currentNode);
    }
  }

  exportProof("dnfcalc");

  return extractDNF(rootNode.get());
}

bool Tableau::applyModalRule() {
  // TODO: hack using that all terms are reduced, s.t. only possible next rule is modal
  while (!unreducedNodes.empty()) {
    auto currentNode = unreducedNodes.top();
    unreducedNodes.pop();
    if (applyRule(currentNode)) {
      return true;
    }
  }
  return false;
}

Clause Tableau::calcReuqest() {
  while (!unreducedNodes.empty()) {
    auto currentNode = unreducedNodes.top();
    unreducedNodes.pop();
    if (currentNode->metastatement) {
      if (currentNode->applyRule<ProofRule::negA>()) {
        break;  // metastatement found and all requests calculated
      }
    } else if (currentNode->relation && currentNode->relation->negated) {
      if (currentNode->applyRule<ProofRule::negA>()) {
        break;
      }
    }
  }

  exportProof("requestcalc");

  // exrtact terms for new node
  Node *node = rootNode.get();
  std::optional<Relation> oldPositive;
  std::optional<Relation> newPositive;
  std::vector<int> activeLabels;
  while (node != nullptr) {  // exploit that only alpha rules applied
    if (node->relation && !node->relation->negated) {
      if (!oldPositive) {
        oldPositive = node->relation;
      } else {
        newPositive = node->relation;
        activeLabels = node->relation->labels();
      }
    }
    node = node->leftNode.get();
  }
  Clause request{*newPositive};
  node = rootNode.get();
  while (node != nullptr) {  // exploit that only alpha rules applied
    if (node->relation && node->relation->negated) {
      std::vector<int> relationLabels = node->relation->labels();
      bool allLabelsActive = true;
      for (auto label : relationLabels) {
        if (find(activeLabels.begin(), activeLabels.end(), label) == activeLabels.end()) {
          allLabelsActive = false;
          break;
        }
      }
      if (allLabelsActive) {
        request.push_back(*node->relation);
      }
    }
    node = node->leftNode.get();
  }
  return request;
}

void Tableau::toDotFormat(std::ofstream &output) const {
  output << "graph {" << std::endl << "node[shape=\"plaintext\"]" << std::endl;
  rootNode->toDotFormat(output);
  output << "}" << std::endl;
}

void Tableau::exportProof(std::string filename) const {
  std::ofstream file(filename + ".dot");
  toDotFormat(file);
  file.close();
}
