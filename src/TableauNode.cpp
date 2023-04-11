#include "Tableau.h"

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

/* RULES */
template <>
std::optional<std::tuple<Relation, Metastatement>> Tableau::Node::applyRule<ProofRule::a>() {
  if (relation && !relation->negated) {
    auto result = relation->applyRuleDeep<ProofRule::a, std::tuple<Relation, Metastatement>>();
    if (result) {
      auto &[newRelation, newMetastatement] = *result;
      appendBranches(newRelation);
      appendBranches(newMetastatement);
      return result;
    }
  }
  return std::nullopt;
}
template <>
std::optional<bool> Tableau::Node::applyRule<ProofRule::id>() {
  if (relation && !relation->negated) {
    auto rId = relation->applyRuleDeep<ProofRule::id, Relation>();
    if (rId) {
      appendBranches(*rId);
      return true;
    }
  }
  return std::nullopt;
}
template <>
std::optional<bool> Tableau::Node::applyRule<ProofRule::at>() {
  if (relation && !relation->negated) {
    auto atResult = relation->applyRuleDeep<ProofRule::at, std::tuple<Relation, Metastatement>>();
    if (atResult) {
      auto &[newRelation, newMetastatement] = *atResult;
      appendBranches(newRelation);
      appendBranches(newMetastatement);
    }
  }
  return std::nullopt;
}
// TODO: meta statemetn adaption :       node->metastatement->label2 =
// std::min(metastatement.label1, metastatement.label2);
template <>
std::optional<bool> Tableau::Node::applyRule<ProofRule::propagation>() {
  bool appliedRule = false;
  if (metastatement && metastatement->type == MetastatementType::labelEquality) {
    Tableau::Node *currentNode = parentNode;
    while (currentNode != nullptr) {
      if (currentNode->relation && currentNode->relation->negated) {
        auto rProp = currentNode->relation->applyRuleDeep<ProofRule::propagation, Relation>(
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
  return appliedRule ? std::make_optional(true) : std::nullopt;
}
template <>
std::optional<bool> Tableau::Node::applyRule<ProofRule::negA>() {
  bool appliedRule = false;
  if (metastatement && metastatement->type == MetastatementType::labelRelation) {
    Tableau::Node *currentNode = parentNode;
    while (currentNode != nullptr) {
      if (currentNode->relation && currentNode->relation->negated) {
        auto rNegA =
            currentNode->relation->applyRuleDeep<ProofRule::negA, Relation>(&(*metastatement));
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
      auto rNegA =
          relation->applyRuleDeep<ProofRule::negA, Relation>(&(*currentMetaNode->metastatement));
      if (rNegA) {
        appliedRule = true;
        rNegA->negated = relation->negated;
        appendBranches(*rNegA);
      }
      currentMetaNode = currentMetaNode->parentMetastatement;
    }
  }
  return appliedRule ? std::make_optional(true) : std::nullopt;
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

bool Tableau::Node::applyDNFRule() {
  auto rId = relation->applyRuleDeep<ProofRule::id, Relation>();
  if (rId) {
    rId->negated = relation->negated;
    appendBranches(*rId);
    return true;
  }
  auto rComposition = relation->applyRuleDeep<ProofRule::composition, Relation>();
  if (rComposition) {
    rComposition->negated = relation->negated;
    appendBranches(*rComposition);
    return true;
  }
  auto rIntersection = relation->applyRuleDeep<ProofRule::intersection, Relation>();
  if (rIntersection) {
    rIntersection->negated = relation->negated;
    appendBranches(*rIntersection);
    return true;
  }
  if (!relation->negated) {
    auto rAt = relation->applyRuleDeep<ProofRule::at, std::tuple<Relation, Metastatement>>();
    if (rAt) {
      auto &[r1, metastatement] = *rAt;
      r1.negated = relation->negated;
      appendBranches(r1);
      appendBranches(metastatement);
      return true;
    }
  } else {
    auto rNegAt = relation->applyRuleDeep<ProofRule::negAt, Relation>();
    if (rNegAt) {
      auto r1 = *rNegAt;
      r1.negated = relation->negated;
      appendBranches(r1);
      return true;
    }
  }
  auto rChoice = relation->applyRuleDeep<ProofRule::choice, std::tuple<Relation, Relation>>();
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
      relation->applyRuleDeep<ProofRule::transitiveClosure, std::tuple<Relation, Relation>>();
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

bool Tableau::Node::applyAnyRule() {
  if (metastatement) {
    if (metastatement->type == MetastatementType::labelRelation) {
      applyRule<ProofRule::negA, bool>();
      return false;  // TODO: this value may be true
    } else {
      applyRule<ProofRule::propagation, bool>();
      return true;
    }
  } else if (relation) {
    if (relation->isNormal()) {
      if (relation->negated) {
        if (applyRule<ProofRule::negA, bool>()) {
          return true;
        }
      } else if (applyRule<ProofRule::a, std::tuple<Relation, Metastatement>>()) {
        return true;
      }
    } else if (applyDNFRule()) {
      return true;
    }
  }
  return false;
}

bool Tableau::Node::apply(const std::initializer_list<ProofRule> rules) {
  for (const auto &rule : rules) {
    switch (rule) {
      case ProofRule::a:
        if (applyRule<ProofRule::a, std::tuple<Relation, Metastatement>>()) {
          return true;
        }
        break;
      case ProofRule::at:
        if (applyRule<ProofRule::at, bool>()) {
          return true;
        }
        break;
      case ProofRule::bottom:
        break;
      case ProofRule::choice:
        break;
      case ProofRule::composition:
        break;
      case ProofRule::converseA:
        break;
      case ProofRule::empty:
        break;
      case ProofRule::id:
        break;
      case ProofRule::intersection:
        break;
      case ProofRule::negA:
        break;
      case ProofRule::negAt:
        break;
      case ProofRule::negChoice:
        break;
      case ProofRule::negComposition:
        break;
      case ProofRule::negConverseA:
        break;
      case ProofRule::negId:
        break;
      case ProofRule::negIntersection:
        break;
      case ProofRule::negTransitiveClosure:
        break;
      case ProofRule::none:
        break;
      case ProofRule::propagation:
        break;
      case ProofRule::transitiveClosure:
        break;
    }
  }
  return false;
}