#include <iostream>
#include <ranges>

#include "Tableau.h"

namespace {
// ---------------------- Anonymous helper functions ----------------------

bool isAppendable(const DNF &dnf) {
  return std::ranges::all_of(dnf, [](const auto &cube) { return !cube.empty(); });
}

// given dnf f and literal l it gives smaller dnf f' such that f & l <-> f'
// it removes cubes with ~l from f
// it removes l from remaining cubes
void reduceDNF(DNF &dnf, const Literal &literal) {
  // remove cubes with literals ~l
  auto [begin, end] = std::ranges::remove_if(dnf, [&](const auto &cube) {
    return std::ranges::any_of(
        cube, [&](const auto &cubeLiteral) { return literal.isNegatedOf(cubeLiteral); });
  });
  dnf.erase(begin, end);

  // remove l from dnf
  for (auto &cube : dnf) {
    auto [begin, end] = std::ranges::remove(cube, literal);
    cube.erase(begin, end);
  }
}

}  // namespace

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

// deletes literals in dnf that are already in prefix
// if negated literal occurs we omit the whole cube
void Tableau::Node::appendBranchInternalUp(DNF &dnf) const {
  const Node *node = this;
  do {
    reduceDNF(dnf, node->literal);
  } while ((node = node->parentNode) != nullptr);
}

void Tableau::Node::appendBranchInternalDown(DNF &dnf) {
  reduceDNF(dnf, literal);

  const bool contradiction = dnf.empty();
  if (contradiction) {
    closeBranch();
    return;
  }
  if (!isAppendable(dnf)) {
    return;
  }

  if (!isLeaf()) {
    // No leaf: descend recursively
    for (const auto &child : children) {
      DNF branchCopy(dnf);
      child->appendBranchInternalDown(branchCopy);  // copy for each branching
    }
    return;
  }

  if (isClosed()) {
    // Closed leaf: nothing to do
    return;
  }

  assert(isLeaf() && !isClosed() && isAppendable(dnf));

  // Open leaf: append
  // transform dnf into a tableau and append it
  for (const auto &cube : dnf) {
    Node *newNode = this;
    for (const auto &literal : cube) {
      newNode = new Node(newNode, literal);
      newNode->parentNode->children.emplace_back(newNode);
      tableau->unreducedNodes.push(newNode);
    }
  }
}

void Tableau::Node::closeBranch() {
  Node *newNode = new Node(this, BOTTOM);
  children.clear();
  children.emplace_back(newNode);
  tableau->unreducedNodes.push(newNode);
}

void Tableau::Node::appendBranch(const DNF &dnf) {
  assert(!dnf.empty());     // empty DNF makes no sense
  assert(dnf.size() <= 2);  // We only support binary branching for now (might change in the future)

  DNF dnfCopy(dnf);
  appendBranchInternalUp(dnfCopy);
  appendBranchInternalDown(dnfCopy);
}

void Tableau::Node::appendBranch(const Cube &cube) { appendBranch(DNF{cube}); }

void Tableau::Node::appendBranch(const Literal &literal) { appendBranch(Cube{literal}); }

std::optional<DNF> Tableau::Node::applyRule(const bool modalRule) {
  auto const result = literal.applyRule(modalRule);
  if (!result) {
    return std::nullopt;
  }

  auto disjunction = *result;
  appendBranch(disjunction);

  // make rule application in-place
  assert(parentNode != nullptr);  // there is always a dummy root node
  if (!children.empty()) {
    // move all other children to parents children
    for (auto &child : children) {
      child->parentNode = parentNode;
    }

    parentNode->children.insert(parentNode->children.end(),
                                std::make_move_iterator(children.begin()),
                                std::make_move_iterator(children.end()));

    auto parentsPointerToThisIt = std::ranges::find_if(
        parentNode->children, [this](auto &element) { return element.get() == this; });
    auto pointerToThis = std::move(*parentsPointerToThisIt);  // ownership of this
    parentNode->children.erase(parentsPointerToThisIt);
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

    appendBranch(substitute(literal, search1, replace1));
    appendBranch(substitute(literal, search2, replace2));
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
