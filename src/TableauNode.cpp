#include <iostream>
#include <ranges>

#include "Tableau.h"
#include "utility.h"

namespace {
// ---------------------- Anonymous helper functions ----------------------

bool isAppendable(const DNF &dnf) {
  return std::ranges::all_of(dnf, [](const auto &cube) { return !cube.empty(); });
}

// given dnf f and literal l it gives smaller dnf f' such that f & l <-> f'
// it removes cubes with ~l from f
// it removes l from remaining cubes
void reduceDNF(DNF &dnf, const Literal &literal) {
  assert(validateDNF(dnf));

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

  assert(validateDNF(dnf));
}

}  // namespace

Tableau::Node::Node(Node *parent, Literal literal)
    : tableau(parent != nullptr ? parent->tableau : nullptr),
      literal(std::move(literal)),
      parentNode(parent) {}

Tableau::Node::~Node() {
  // remove this from unreducedNodes
  tableau->unreducedNodes.erase(this);
}

bool Tableau::Node::validate() const {
  if (tableau == nullptr) {
    std::cout << "Invalid node(no tableau) " << this << ": " << literal.toString() << std::endl;
    return false;
  }
  if (tableau->rootNode.get() != this && parentNode == nullptr) {
    std::cout << "Invalid node(no parent) " << this << ": " << literal.toString() << std::endl;
    return false;
  }
  return literal.validate();
}

bool Tableau::Node::validateRecursive() const {
  return validate() && std::ranges::all_of(children, [](auto &child) { return child->validate(); });
}

// TODO: lazy evaluation + save intermediate results (evaluate each node at most once)
bool Tableau::Node::isClosed() const {
  if (literal == BOTTOM) {
    return true;
  }
  if (children.empty()) {
    return false;
  }
  return std::ranges::all_of(children, &Node::isClosed);
}

bool Tableau::Node::isLeaf() const { return children.empty(); }

// deletes literals in dnf that are already in prefix
// if negated literal occurs we omit the whole cube
void Tableau::Node::appendBranchInternalUp(DNF &dnf) const {
  const Node *node = this;
  do {
    assert(validateDNF(dnf));
    reduceDNF(dnf, node->literal);
  } while ((node = node->parentNode) != nullptr);
}

void Tableau::Node::appendBranchInternalDown(DNF &dnf) {
  assert(tableau->unreducedNodes.validate());
  assert(validateDNF(dnf));
  reduceDNF(dnf, literal);

  const bool contradiction = dnf.empty();
  if (contradiction) {
    closeBranch();
    assert(tableau->unreducedNodes.validate());
    return;
  }
  if (!isAppendable(dnf)) {
    assert(tableau->unreducedNodes.validate());
    return;
  }

  if (!isLeaf()) {
    // No leaf: descend recursively
    // Copy DNF for all children but the last one (for the last one we can reuse the DNF).
    for (const auto &child : std::ranges::drop_view(children, 1)) {
      DNF branchCopy(dnf);
      child->appendBranchInternalDown(branchCopy);  // copy for each branching
    }
    children[0]->appendBranchInternalDown(dnf);
    assert(tableau->unreducedNodes.validate());
    return;
  }

  if (isClosed()) {
    // Closed leaf: nothing to do
    assert(tableau->unreducedNodes.validate());
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
  assert(tableau->unreducedNodes.validate());
}

void Tableau::Node::closeBranch() {
  assert(tableau->unreducedNodes.validate());
  // It is safe to clear the children: the Node destructor
  // will make sure to remove them from worklist
  children.clear();
  assert(tableau->unreducedNodes.validate());  // validate that it was indeed safe to clear
  children.emplace_back(new Node(this, BOTTOM));
}

void Tableau::Node::appendBranch(const DNF &dnf) {
  assert(tableau->unreducedNodes.validate());
  assert(validateDNF(dnf));
  assert(!dnf.empty());     // empty DNF makes no sense
  assert(dnf.size() <= 2);  // We only support binary branching for now (might change in the future)

  DNF dnfCopy(dnf);
  appendBranchInternalUp(dnfCopy);
  appendBranchInternalDown(dnfCopy);

  assert(tableau->unreducedNodes.validate());
}

void Tableau::Node::dnfBuilder(DNF &dnf) const {
  if (isClosed()) {
    return;
  }

  for (const auto &child : children) {
    DNF childDNF;
    child->dnfBuilder(childDNF);
    dnf.insert(dnf.end(), std::make_move_iterator(childDNF.begin()),
               std::make_move_iterator(childDNF.end()));
  }

  if (!literal.isNormal()) {
    // Ignore non-normal literals.
    return;
  }

  if (isLeaf()) {
    dnf.push_back({literal});
  } else {
    if (dnf.empty()) {
      dnf.emplace_back();
    }
    for (auto &cube : dnf) {
      cube.push_back(literal);
    }
  }
}

DNF Tableau::Node::extractDNF() const {
  DNF dnf;
  dnfBuilder(dnf);
  return dnf;
}

std::optional<DNF> Tableau::Node::applyRule(const bool modalRule) {
  auto const result = Rules::applyRule(literal, modalRule);
  if (!result) {
    return std::nullopt;
  }

  auto disjunction = *result;
  appendBranch(disjunction);

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
    assert(edgeLiteral.validate());
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
    if (lit.negated) {
      continue;
    }

    // Normal and positive literal: collect new labels
    for (auto newLabel : lit.labels()) {
      push_back_unique(labels, newLabel);
    }
  }

  for (const auto label : labels) {
    // T -> e
    const CanonicalSet search = Set::fullSet();
    const CanonicalSet replace = Set::newEvent(label);
    for (auto &lit : substitute(literal, search, replace)) {
      appendBranch(lit);
    }
  }
}

void Tableau::Node::inferModalAtomic() {
  const Literal &edgeLiteral = literal;
  // (e1, e2) \in b
  assert(edgeLiteral.validate());
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
  const CanonicalSet top = Set::fullSet();

  const Node *cur = this;
  while ((cur = cur->parentNode) != nullptr) {
    tableau->exportDebug("debug");
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
    // cases for Top -> e
    // replace T -> e
    for (auto &lit : substitute(curLit, top, replace1)) {
      appendBranch(lit);
    }
    for (auto &lit : substitute(curLit, top, replace2)) {
      appendBranch(lit);
    }
  }
}

// FIXME: use or remove
void Tableau::Node::replaceNegatedTopOnBranch(const std::vector<int> &labels) {
  const Node *cur = this;
  while ((cur = cur->parentNode) != nullptr) {
    const Literal &curLit = cur->literal;
    if (!curLit.negated || !curLit.isNormal()) {
      continue;
    }
    // replace T -> e
    const CanonicalSet top = Set::fullSet();
    for (const auto label : labels) {
      const CanonicalSet e = Set::newEvent(label);
      for (auto &lit : substitute(curLit, top, e)) {
        appendBranch(lit);
      }
    }
  }
}

void Tableau::Node::toDotFormat(std::ofstream &output) const {
  // tooltip
  output << "N" << this << "[tooltip=\"";
  output << this << "\n\n";  // address
  if (literal.operation == PredicateOperation::setNonEmptiness && literal.negated) {
    output << "Annotation: \n";
    output << Annotated::toString<true>(literal.annotatedSet());  // annotation id
    output << "\n\n";
    output << Annotated::toString<false>(literal.annotatedSet());  // annotation base
  }

  // label
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
