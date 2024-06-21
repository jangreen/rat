#include <iostream>

#include "Tableau.h"

Tableau::Node::Node(Node *parent, const Literal &literal)
    : tableau(parent != nullptr ? parent->tableau : nullptr),
      literal(literal),
      parentNode(parent) {}

// TODO: lazy evaluation + save intermediate results (evaluate each node at most once)
bool Tableau::Node::isClosed() const {
  if (literal == BOTTOM) {
    return true;
  }
  return leftNode != nullptr && leftNode->isClosed() &&
         (rightNode == nullptr || rightNode->isClosed());
}

bool Tableau::Node::isLeaf() const { return (leftNode == nullptr) && (rightNode == nullptr); }

bool Tableau::Node::branchContains(const Literal &lit) {
  Literal negatedCopy(lit);
  negatedCopy.negated = !negatedCopy.negated;

  const Node *node = this;
  do {
    if (node->literal == lit) {
      // Found literal
      return true;
    } else if (lit.operation != PredicateOperation::setNonEmptiness &&
               node->literal == negatedCopy) {
      // Found negated literal of atomic predicate
      // Rule (\bot_0): check if p & ~p (only in case of atomic predicate)
      Node newNode(this, lit);
      Node newNodeBot(&newNode, std::move(Literal(BOTTOM)));
      newNode.leftNode = std::make_unique<Node>(std::move(newNodeBot));
      leftNode = std::make_unique<Node>(std::move(newNode));
      return true;
    }
  } while ((node = node->parentNode) != nullptr);

  return false;
}

void Tableau::Node::appendBranch(const DNF &dnf) {
  assert(!dnf.empty());     // empty DNF makes no sense
  assert(dnf.size() <= 2);  // We only support binary branching for now (might change in the future)

  if (!isLeaf()) {
    // No leaf: descend recursively
    if (leftNode != nullptr) {
      leftNode->appendBranch(dnf);
    }
    if (rightNode != nullptr) {
      rightNode->appendBranch(dnf);
    }
    return;
  }

  if (isClosed()) {
    // Closed leaf: nothing to do
    return;
  }

  // Open leaf: append
  if (dnf.size() == 2) {
    // only append if all resulting branches have new literals
    if (!appendable(dnf[0]) || !appendable(dnf[1])) {
      return;
    }

    // trick: lift disjunctive appendBranch to sets
    for (const auto &lit : dnf[1]) {
      appendBranch(lit);
    }
    auto temp = std::move(leftNode);
    leftNode = nullptr;
    for (const auto &lit : dnf[0]) {
      appendBranch(lit);
    }
    rightNode = std::move(temp);
  } else {
    assert(dnf.size() == 1);
    for (const auto &lit : dnf[0]) {
      appendBranch(lit);
    }
  }
}

bool Tableau::Node::appendable(const Cube &cube) {
  // assume that it is called only on leafs
  // predicts conjunctive literal appending returns true if it would append at least some literal
  assert(isLeaf() && !isClosed());
  for (const auto &lit : cube) {
    if (!branchContains(lit)) {
      return true;
    }
  }
  return false;
}

void Tableau::Node::appendBranch(const Literal &leftLiteral) {
  if (!isLeaf()) {
    // No leaf: descend recursively
    if (leftNode != nullptr) {
      leftNode->appendBranch(leftLiteral);
    }
    if (rightNode != nullptr) {
      rightNode->appendBranch(leftLiteral);
    }
    return;
  }

  if (isClosed() || branchContains(leftLiteral)) {
    // Closed leaf or literal already exists: do nothing
    return;
  }

  // Open leaf and new literal: append
  Node newNode(this, leftLiteral);
  leftNode = std::make_unique<Node>(std::move(newNode));
  tableau->unreducedNodes.push(leftNode.get());
}

// FIXME make faster
void Tableau::Node::appendBranch(const Literal &leftLiteral, const Literal &rightLiteral) {
  if (!isLeaf()) {
    // No leaf: descend recursively
    if (leftNode != nullptr) {
      leftNode->appendBranch(leftLiteral, rightLiteral);
    }
    if (rightNode != nullptr) {
      rightNode->appendBranch(leftLiteral, rightLiteral);
    }
    return;
  }

  if (isClosed()) {
    // Closed leaf: nothing to do
    return;
  }

  // Open leaf: append literals if not already contained
  if (!branchContains(leftLiteral)) {
    Node newNode(this, leftLiteral);
    leftNode = std::make_unique<Node>(std::move(newNode));
    tableau->unreducedNodes.push(leftNode.get());
  }
  if (!branchContains(rightLiteral)) {
    Node newNode(this, rightLiteral);
    rightNode = std::make_unique<Node>(std::move(newNode));
    tableau->unreducedNodes.push(rightNode.get());
  }
}

// FIXME use in place
std::optional<DNF> Tableau::Node::applyRule(bool modalRule) {
  auto result = literal.applyRule(modalRule);
  if (!result) {
    return std::nullopt;
  }

  auto disjunction = *result;
  appendBranch(disjunction);
  // // make rule application in-place
  // if (parentNode != nullptr && parentNode->rightNode == nullptr) {
  //   // important: do right first because setting parentNode->leftNode destroys 'this'
  //   if (rightNode != nullptr) {
  //     rightNode->parentNode = parentNode;
  //     parentNode->rightNode = std::move(rightNode);
  //   }
  //   if (leftNode != nullptr) {
  //     leftNode->parentNode = parentNode;
  //     parentNode->leftNode = std::move(leftNode);
  //   }
  // }

  return disjunction;
}

void Tableau::Node::inferModal() {
  if (!literal.negated) {
    return;
  }

  // Traverse bottom-up
  Node *cur = this;
  while ((cur = cur->parentNode) != nullptr) {
    if (!cur->literal.isNormal() || !cur->literal.isPositiveEdgePredicate()) {
      continue;
    }

    // Normal and positive edge literal
    // check if inside literal can be something inferred
    Literal edgeLiteral = cur->literal;
    // (e1, e2) \in b
    CanonicalSet e1 = Set::newEvent(*edgeLiteral.leftLabel);
    CanonicalSet e2 = Set::newEvent(*edgeLiteral.rightLabel);
    CanonicalRelation b = Relation::newBaseRelation(*edgeLiteral.identifier);
    CanonicalSet e1b = Set::newSet(SetOperation::image, e1, b);
    CanonicalSet be2 = Set::newSet(SetOperation::domain, e2, b);

    CanonicalSet search1 = e1b;
    CanonicalSet replace1 = e2;
    CanonicalSet search2 = be2;
    CanonicalSet replace2 = e1;

    auto newLiterals = substitute(literal, search1, replace1);
    for (auto &lit : newLiterals) {
      appendBranch(lit);
    }
    auto newLiterals2 = substitute(literal, search2, replace2);
    for (auto &lit : newLiterals2) {
      appendBranch(lit);
    }
  }
}

void Tableau::Node::inferModalTop() {
  if (!literal.negated) {
    return;
  }

  // get all labels
  Node *cur = this;
  std::vector<int> labels;
  while ((cur = cur->parentNode) != nullptr) {
    const Literal &lit = cur->literal;
    if (!lit.isNormal() || lit.negated) {
      continue;
    }

    // Normal and positive literal: collect new labels
    for (auto newLabel : lit.labels()) {
      if (std::find(labels.begin(), labels.end(), newLabel) == labels.end()) {
        labels.push_back(newLabel);
      }
    }
  }

  for (auto label : labels) {
    // T -> e
    CanonicalSet search = Set::newSet(SetOperation::full);
    CanonicalSet replace = Set::newEvent(label);

    auto newLiterals = substitute(literal, search, replace);
    for (auto &lit : newLiterals) {
      appendBranch(lit);
    }
  }
}

void Tableau::Node::inferModalAtomic() {
  Literal edgeLiteral = literal;
  // (e1, e2) \in b
  CanonicalSet e1 = Set::newEvent(*edgeLiteral.leftLabel);
  CanonicalSet e2 = Set::newEvent(*edgeLiteral.rightLabel);
  CanonicalRelation b = Relation::newBaseRelation(*edgeLiteral.identifier);
  CanonicalSet e1b = Set::newSet(SetOperation::image, e1, b);
  CanonicalSet be2 = Set::newSet(SetOperation::domain, e2, b);

  CanonicalSet search1 = e1b;
  CanonicalSet replace1 = e2;
  CanonicalSet search2 = be2;
  CanonicalSet replace2 = e1;
  // replace T -> e
  CanonicalSet search12 = Set::newSet(SetOperation::full);

  Node *cur = this;
  while ((cur = cur->parentNode) != nullptr) {
    const Literal &curLit = cur->literal;
    if (!curLit.negated || !curLit.isNormal()) {
      continue;
    }

    // Negated and normal lit
    // check if inside literal can be something inferred
    const auto newLiterals = substitute(curLit, search1, replace1);
    for (auto &lit : newLiterals) {
      appendBranch(lit);
    }
    const auto newLiterals2 = substitute(curLit, search2, replace2);
    for (auto &lit : newLiterals2) {
      appendBranch(lit);
    }
    const auto newLiterals3 = substitute(curLit, search12, replace1);
    for (auto &lit : newLiterals3) {
      appendBranch(lit);
    }
    const auto newLiterals4 = substitute(curLit, search12, replace2);
    for (auto &lit : newLiterals4) {
      appendBranch(lit);
    }
  }
}

void Tableau::Node::toDotFormat(std::ofstream &output) const {
  output << "N" << this << "[label=\"" << literal.toString() << "\"";
  // color closed branches
  if (isClosed()) {
    output << ", fontcolor=green";
  }
  output << "];" << std::endl;
  // children
  if (leftNode != nullptr) {
    leftNode->toDotFormat(output);
    output << "N" << this << " -- " << "N" << leftNode << ";" << std::endl;
  }
  if (rightNode != nullptr) {
    rightNode->toDotFormat(output);
    output << "N" << this << " -- " << "N" << rightNode << ";" << std::endl;
  }
}
