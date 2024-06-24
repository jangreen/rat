#include <iostream>

#include "Tableau.h"

Tableau::Node::Node(Node *parent, Literal literal)
    : tableau(parent != nullptr ? parent->tableau : nullptr),
      literal(std::move(literal)),
      parentNode(parent) {}

// TODO: lazy evaluation + save intermediate results (evaluate each node at most once)
bool Tableau::Node::isClosed() const {
  if (literal == BOTTOM) {
    return true;
  }
  if (children.empty()) {
    return false;
  }
  for (const auto &child : children) {
    if (!child->isClosed()) {
      return false;
    }
  }
  return true;
}

bool Tableau::Node::isLeaf() const { return children.empty(); }

bool Tableau::Node::branchContains(const Literal &lit) {
  Literal negatedCopy(lit);
  negatedCopy.negated = !negatedCopy.negated;

  const Node *node = this;
  do {
    if (node->literal == lit) {
      // Found literal
      return true;
    }

    if (lit.operation != PredicateOperation::setNonEmptiness && node->literal == negatedCopy) {
      // Found negated literal of atomic predicate
      // Rule (\bot_0): check if p & ~p (only in case of atomic predicate)
      auto *newNode = new Node(this, lit);
      auto *newNodeBot = new Node(newNode, BOTTOM);
      children.emplace_back(newNodeBot);
      newNode->children.emplace_back(newNode);
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
    for (const auto &child : children) {
      child->appendBranch(dnf);
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
    auto temp = std::move(children.back());
    children.pop_back();
    for (const auto &lit : dnf[0]) {
      appendBranch(lit);
    }
    children.push_back(std::move(temp));
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
  return std::ranges::any_of(cube, [&](auto lit) { return !branchContains(lit); });
}

void Tableau::Node::appendBranch(const Literal &leftLiteral) {
  if (!isLeaf()) {
    // No leaf: descend recursively
    for (const auto &child : children) {
      child->appendBranch(literal);
    }
    return;
  }

  if (isClosed() || branchContains(leftLiteral)) {
    // Closed leaf or literal already exists: do nothing
    return;
  }

  // Open leaf and new literal: append
  Node *newNode = new Node(this, leftLiteral);
  children.emplace_back(newNode);
  tableau->unreducedNodes.push(newNode);
}

// FIXME make faster
void Tableau::Node::appendBranch(const Literal &leftLiteral, const Literal &rightLiteral) {
  if (!isLeaf()) {
    // No leaf: descend recursively
    for (const auto &child : children) {
      child->appendBranch(leftLiteral, rightLiteral);
    }
    return;
  }

  if (isClosed()) {
    // Closed leaf: nothing to do
    return;
  }

  // Open leaf: append literals if not already contained
  if (!branchContains(leftLiteral)) {
    Node *newNode = new Node(this, leftLiteral);
    children.emplace_back(newNode);
    tableau->unreducedNodes.push(newNode);
  }
  if (!branchContains(rightLiteral)) {
    Node *newNode = new Node(this, rightLiteral);
    children.emplace_back(newNode);
    tableau->unreducedNodes.push(newNode);
  }
}

std::optional<DNF> Tableau::Node::applyRule(const bool modalRule) {
  auto const result = literal.applyRule(modalRule);
  if (!result) {
    return std::nullopt;
  }

  auto disjunction = *result;
  appendBranch(disjunction);
  // make rule application in-place
  if (parentNode != nullptr) {
    for (auto &child : children) {
      child->parentNode = parentNode;
    }

    parentNode->children.insert(parentNode->children.end(),
                                std::make_move_iterator(children.begin()),
                                std::make_move_iterator(children.end()));
    // next line destroys this
    parentNode->children.erase(
        std::remove_if(std::begin(parentNode->children), std::end(parentNode->children),
                       [this](auto &element) { return element.get() == this; }));
  }

  return disjunction;
}

void Tableau::Node::inferModal() {
  if (!literal.negated) {
    return;
  }

  // Traverse bottom-up
  const Node *cur = this;
  while ((cur = cur->parentNode) != nullptr) {
    if (!cur->literal.isNormal() || !cur->literal.isPositiveEdgePredicate()) {
      continue;
    }

    // Normal and positive edge literal
    // check if inside literal can be something inferred
    const Literal &edgeLiteral = cur->literal;
    // (e1, e2) \in b
    const CanonicalSet e1 = Set::newEvent(*edgeLiteral.leftLabel);
    const CanonicalSet e2 = Set::newEvent(*edgeLiteral.rightLabel);
    const CanonicalRelation b = Relation::newBaseRelation(*edgeLiteral.identifier);
    const CanonicalSet e1b = Set::newSet(SetOperation::image, e1, b);
    const CanonicalSet be2 = Set::newSet(SetOperation::domain, e2, b);

    const CanonicalSet search1 = e1b;
    const CanonicalSet replace1 = e2;
    const CanonicalSet search2 = be2;
    const CanonicalSet replace2 = e1;

    for (auto &lit : substitute(literal, search1, replace1)) {
      appendBranch(lit);
    }
    for (auto &lit : substitute(literal, search2, replace2)) {
      appendBranch(lit);
    }
  }
}

void Tableau::Node::inferModalTop() {
  if (!literal.negated) {
    return;
  }

  // get all labels
  const Node *cur = this;
  std::vector<int> labels;
  while ((cur = cur->parentNode) != nullptr) {
    const Literal &lit = cur->literal;
    if (!lit.isNormal() || lit.negated) {
      continue;
    }

    // Normal and positive literal: collect new labels
    for (auto newLabel : lit.labels()) {
      if (std::ranges::find(labels, newLabel) == labels.end()) {
        labels.push_back(newLabel);
      }
    }
  }

  for (const auto label : labels) {
    // T -> e
    const CanonicalSet search = Set::newSet(SetOperation::full);
    const CanonicalSet replace = Set::newEvent(label);
    for (auto &lit : substitute(literal, search, replace)) {
      appendBranch(lit);
    }
  }
}

void Tableau::Node::inferModalAtomic() {
  const Literal &edgeLiteral = literal;
  // (e1, e2) \in b
  const CanonicalSet e1 = Set::newEvent(*edgeLiteral.leftLabel);
  const CanonicalSet e2 = Set::newEvent(*edgeLiteral.rightLabel);
  const CanonicalRelation b = Relation::newBaseRelation(*edgeLiteral.identifier);
  const CanonicalSet e1b = Set::newSet(SetOperation::image, e1, b);
  const CanonicalSet be2 = Set::newSet(SetOperation::domain, e2, b);

  const CanonicalSet search1 = e1b;
  const CanonicalSet replace1 = e2;
  const CanonicalSet search2 = be2;
  const CanonicalSet replace2 = e1;
  // replace T -> e
  const CanonicalSet search12 = Set::newSet(SetOperation::full);

  const Node *cur = this;
  while ((cur = cur->parentNode) != nullptr) {
    const Literal &curLit = cur->literal;
    if (!curLit.negated || !curLit.isNormal()) {
      continue;
    }

    // Negated and normal lit
    // check if inside literal can be something inferred
    for (auto &lit : substitute(curLit, search1, replace1)) {
      appendBranch(lit);
    }
    for (auto &lit : substitute(curLit, search2, replace2)) {
      appendBranch(lit);
    }
    for (auto &lit : substitute(curLit, search12, replace1)) {
      appendBranch(lit);
    }
    for (auto &lit : substitute(curLit, search12, replace2)) {
      appendBranch(lit);
    }
  }
}

void Tableau::Node::toDotFormat(std::ofstream &output) const {
  output << "N" << this << "[tooltip=\"";
  // debug
  output << this << std::endl << std::endl;
  // label/cube
  output << "\",label=\"" << literal.toString() << "\"";
  // color closed branches
  if (isClosed()) {
    output << ", fontcolor=green";
  }
  output << "];" << std::endl;
  // children
  for (const auto &child : children) {
    child->toDotFormat(output);
    output << "N" << this << " -- " << "N" << child << ";" << std::endl;
  }
}
