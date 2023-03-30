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

bool Tableau::Node::applyDNFRule() {
  auto rId = relation->applyRuleRecursive<ProofRule::id, Relation>();
  if (rId) {
    rId->negated = relation->negated;
    appendBranches(*rId);
    return true;
  }
  auto rComposition = relation->applyRuleRecursive<ProofRule::composition, Relation>();
  if (rComposition) {
    rComposition->negated = relation->negated;
    appendBranches(*rComposition);
    return true;
  }
  auto rIntersection = relation->applyRuleRecursive<ProofRule::intersection, Relation>();
  if (rIntersection) {
    rIntersection->negated = relation->negated;
    appendBranches(*rIntersection);
    return true;
  }
  if (!relation->negated) {
    auto rAt = relation->applyRuleRecursive<ProofRule::at, std::tuple<Relation, Metastatement>>();
    if (rAt) {
      auto &[r1, metastatement] = *rAt;
      r1.negated = relation->negated;
      appendBranches(r1);
      appendBranches(metastatement);
      return true;
    }
  } else {
    auto rNegAt = relation->applyRuleRecursive<ProofRule::negAt, Relation>();
    if (rNegAt) {
      auto r1 = *rNegAt;
      r1.negated = relation->negated;
      appendBranches(r1);
      return true;
    }
  }
  auto rChoice = relation->applyRuleRecursive<ProofRule::choice, std::tuple<Relation, Relation>>();
  if (rChoice) {
    auto &[r1, r2] = *rChoice;
    r1.negated = relation->negated;
    r2.negated = relation->negated;
    if (relation->negated) {
      appendBranches(r1);
      appendBranches(r2);
    } else {
      appendBranches(r1, r2);
    }
    return true;
  }

  auto rTransitiveClosure =
      relation->applyRuleRecursive<ProofRule::transitiveClosure, std::tuple<Relation, Relation>>();
  if (rTransitiveClosure) {
    auto &[r1, r2] = *rTransitiveClosure;
    r1.negated = relation->negated;
    r2.negated = relation->negated;
    if (relation->negated) {
      appendBranches(r1);
      appendBranches(r2);
    } else {
      appendBranches(r1, r2);
    }
    return true;
  }
  return false;
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

// TODO: move in Node class
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
    } else if (node->applyDNFRule()) {
      return true;
    }
  }
  return false;
}

bool Tableau::solve(int bound) {
  while (!unreducedNodes.empty() && bound > 0) {
    bound--;
    auto currentNode = unreducedNodes.top();
    unreducedNodes.pop();
    // exportProof("infinite"); // TODO: remove
    applyRule(currentNode);
  }
}

bool Tableau::applyModalRule() {
  // assuming that all terms are reduced, s.t. only possible next rule is modal
  while (!unreducedNodes.empty()) {
    auto currentNode = unreducedNodes.top();
    unreducedNodes.pop();
    if (applyRule(currentNode)) {
      // if at rule can be applied do it directly
      // TODO: check this is this the best way to do this? ----
      Node *node = currentNode;
      while (node->leftNode != nullptr) {
        node = &(*node->leftNode);
      }
      Node *newNodeAtCheck = node->parentNode;
      auto atResult =
          newNodeAtCheck->relation
              ->applyRuleRecursive<ProofRule::at, std::tuple<Relation, Metastatement>>();
      if (atResult) {
        const auto &[relation, metastatement] = *atResult;
        node->metastatement->label2 = std::min(metastatement.label1, metastatement.label2);
        node->appendBranches(relation);
        // TODO dont needed?!: node->appendBranches(metastatement);
      }
      // ----
      return true;
    }
  }
  return false;
}

std::tuple<Clause, Clause> Tableau::extractRequest() {
  // exrtact terms for new node
  Node *node = rootNode.get();
  std::optional<Relation> oldPositive;
  std::optional<Relation> newPositive;
  std::vector<int> activeLabels;
  std::vector<int> oldActiveLabels;
  while (node != nullptr) {  // exploit that only alpha rules applied
    if (node->relation && !node->relation->negated) {
      if (!oldPositive) {
        oldPositive = node->relation;
        oldActiveLabels = node->relation->labels();
      } else {
        newPositive = node->relation;
        activeLabels = node->relation->labels();
      }
    }
    node = node->leftNode.get();
  }

  Clause request{*newPositive};
  Clause converseRequest;
  node = rootNode.get();
  while (node != nullptr) {  // exploit that only alpha rules applied
    if (node->relation && node->relation->negated) {
      std::vector<int> relationLabels = node->relation->labels();
      bool allLabelsActive = true;
      bool allLabelsActiveInOld = true;
      for (auto label : relationLabels) {
        if (std::find(activeLabels.begin(), activeLabels.end(), label) == activeLabels.end()) {
          allLabelsActive = false;
          break;
        }
      }
      for (auto label : relationLabels) {
        if (std::find(oldActiveLabels.begin(), oldActiveLabels.end(), label) ==
            oldActiveLabels.end()) {
          allLabelsActiveInOld = false;
          break;
        }
      }
      if (allLabelsActive) {
        request.push_back(*node->relation);
      }
      if (allLabelsActiveInOld) {
        converseRequest.push_back(*node->relation);
      }
    }
    node = node->leftNode.get();
  }
  std::tuple<Clause, Clause> result{request, converseRequest};
  return result;
}

std::tuple<Clause, Clause> Tableau::calcReuqest() {
  while (!unreducedNodes.empty()) {
    auto currentNode = unreducedNodes.top();
    unreducedNodes.pop();
    if (currentNode->metastatement) {
      currentNode->applyRule<ProofRule::negA>();
    } else if (currentNode->relation && currentNode->relation->negated) {
      currentNode->applyRule<ProofRule::negA>();
    }
  }

  exportProof("requestcalc");  // TODO: remove

  return extractRequest();
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
