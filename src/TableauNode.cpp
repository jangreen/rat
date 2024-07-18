#include "TableauNode.h"

#include <iostream>
#include <ranges>

#include "Rules.h"
#include "Tableau.h"
#include "utility.h"

namespace {
// ---------------------- Anonymous helper functions ----------------------

bool isAppendable(const DNF &dnf) {
  return std::ranges::all_of(dnf, [](const auto &cube) { return !cube.empty(); });
}

void reduceDNFAtAWorldCycle(DNF &dnf, const Node *transitiveClosureNode) {
  if (transitiveClosureNode == nullptr) {
    return;
  }
  assert(transitiveClosureNode->validate());

  auto [begin, end] = std::ranges::remove_if(dnf, [&](const auto &cube) {
    return std::ranges::any_of(cube, [&](const Literal &cubeLit) {
      assert(cubeLit.validate());
      return cubeLit == transitiveClosureNode->getLiteral();
    });
  });
  dnf.erase(begin, end);
  reduceDNFAtAWorldCycle(dnf, transitiveClosureNode->getLastUnrollingParent());
}

// given dnf f and literal l it gives smaller dnf f' such that f & l <-> f'
// it removes cubes with ~l from f
// it removes l from remaining cubes
void reduceDNF(DNF &dnf, const Literal &literal) {
  assert(validateDNF(dnf));

  // remove cubes with literals ~l
  auto [begin, end] = std::ranges::remove_if(
      dnf, [&](const auto &cube) { return cubeHasNegatedLiteral(cube, literal); });
  dnf.erase(begin, end);

  // remove l from dnf
  for (auto &cube : dnf) {
    auto [begin, end] = std::ranges::remove(cube, literal);
    cube.erase(begin, end);
  }

  assert(validateDNF(dnf));
}

// TODO: give better name
Cube substitute(const Literal &literal, const CanonicalSet search, const CanonicalSet replace) {
  int c = 1;
  Literal copy = literal;
  Cube newLiterals;
  while (copy.substitute(search, replace, c)) {
    newLiterals.push_back(copy);
    copy = literal;
    c++;
  }
  return newLiterals;
}

}  // namespace

Node::Node(Node *parent, Literal literal)
    : _isClosed(false),
      literal(std::move(literal)),
      parentNode(parent),
      tableau(parent != nullptr ? parent->tableau : nullptr) {
  if (parent != nullptr) {
    parent->children.emplace_back(this);

    activeEvents = parent->activeEvents;
    activeEventBasePairs = parent->activeEventBasePairs;
  }

  if (!literal.negated) {
    activeEvents.merge(literal.events());
    activeEventBasePairs.merge(literal.labelBaseCombinations());
  }
}

Node::~Node() {
  // remove this from unreducedNodes
  tableau->unreducedNodes.erase(this);
}

bool Node::validate() const {
  if (tableau == nullptr) {
    std::cout << "Invalid node(no tableau) " << this << ": " << literal.toString() << std::endl;
    return false;
  }
  if (tableau->getRoot() != this && parentNode == nullptr) {
    std::cout << "Invalid node(no parent) " << this << ": " << literal.toString() << std::endl;
    return false;
  }
  return literal.validate();
}

bool Node::validateRecursive() const {
  assert(validate());
  assert(std::ranges::all_of(children, [](auto &child) { return child->validate(); }));
  return true;
}

void Node::newChild(std::unique_ptr<Node> child) {
  child->parentNode = this;
  child->activeEvents.merge(activeEvents);
  children.push_back(std::move(child));
}

void Node::newChildren(std::vector<std::unique_ptr<Node>> newChildren) {
  for (const auto &newChild : newChildren) {
    newChild->parentNode = this;
  }
  children.insert(children.end(), std::make_move_iterator(newChildren.begin()),
                  std::make_move_iterator(newChildren.end()));
}

std::unique_ptr<Node> Node::removeChild(Node *child) {
  const auto firstUnsharedNodeIt =
      std::ranges::find_if(children, [&](const auto &curChild) { return curChild.get() == child; });
  auto removedChild = std::move(*firstUnsharedNodeIt);
  children.erase(firstUnsharedNodeIt);
  return removedChild;
}

bool Node::isLeaf() const { return children.empty(); }

void Node::rename(const Renaming &renaming) {
  literal.rename(renaming);

  EventSet renamedEvents;
  for (const auto &event : activeEvents) {
    renamedEvents.insert(renaming.rename(event));
  }
  activeEvents = std::move(renamedEvents);
}

// deletes literals in dnf that are already in prefix
// if negated literal occurs we omit the whole cube
void Node::appendBranchInternalUp(DNF &dnf) const {
  auto node = this;
  do {
    assert(validateDNF(dnf));
    if (!isAppendable(dnf)) {
      return;
    }
    reduceDNF(dnf, node->literal);
  } while ((node = node->parentNode) != nullptr);
}

void Node::reduceBranchInternalDown(Cube &cube) {
  assert(tableau->unreducedNodes.validate());

  if (isClosed()) {
    return;
  }

  if (cubeHasNegatedLiteral(cube, literal)) {
    closeBranch();
    return;
  }

  // IMPORTANT: This loop mitigates against the fact that recursive calls can delete
  // children, potentially invalidating the iterator.
  for (int i = children.size() - 1; i >= 0; i--) {
    if (!children.empty()) {
      children[i]->reduceBranchInternalDown(cube);
    }
  }

  const auto litIt = std::ranges::find(cube, literal);
  const bool cubeContainsLiteral = litIt != cube.end();
  if (cubeContainsLiteral) {
    // choose minimal annotation
    litIt->annotation = Annotation::min(litIt->annotation, literal.annotation);
    tableau->removeNode(this);
  }
}

void Node::appendBranchInternalDown(DNF &dnf) {
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

  // Open leaf
  assert(isLeaf() && !isClosed());
  assert(isAppendable(dnf));

  // filter non-active negated literals
  for (auto &cube : dnf) {
    filterNegatedLiterals(cube, activeEvents);
    // TODO: labelBase optimization
    // filterNegatedLiterals(cube, activeEventBasePairs);
  }
  if (!isAppendable(dnf)) {
    return;
  }
  assert(isAppendable(dnf));

  // append: transform dnf into a tableau and append it
  for (const auto &cube : dnf) {
    auto newNode = this;
    for (const auto &literal : cube) {
      newNode = new Node(newNode, literal);
      newNode->lastUnrollingParent = transitiveClosureNode;
      tableau->unreducedNodes.push(newNode);
    }
  }
  assert(tableau->unreducedNodes.validate());
}

void Node::closeBranch() {
  assert(this != tableau->getRoot());
  assert(tableau->unreducedNodes.validate());
  // It is safe to clear the children: the Node destructor
  // will make sure to remove them from worklist
  children.clear();
  assert(tableau->unreducedNodes.validate());  // validate that it was indeed safe to clear
  const auto bottom = new Node(this, BOTTOM);
  bottom->_isClosed = true;

  // update isClosed cache
  auto cur = this;
  while (cur != nullptr) {
    if (std::ranges::any_of(cur->children, [](const auto &child) { return !child->isClosed(); })) {
      break;
    }
    cur->_isClosed = true;
    cur = cur->parentNode;
  }
}

void Node::appendBranch(const DNF &dnf) {
  assert(tableau->unreducedNodes.validate());
  assert(validateDNF(dnf));
  assert(!dnf.empty());     // empty DNF makes no sense
  assert(dnf.size() <= 2);  // We only support binary branching for now (might change in the future)

  if (isClosed()) {
    return;
  }

  DNF dnfCopy(dnf);
  reduceDNFAtAWorldCycle(dnfCopy, transitiveClosureNode);
  appendBranchInternalUp(dnfCopy);

  // empy dnf
  const bool contradiction = dnfCopy.empty();
  if (contradiction) {
    closeBranch();
    return;
  }

  // conjunctive
  const bool isConjunctive = dnfCopy.size() == 1;
  if (isConjunctive) {
    auto &cube = dnfCopy.at(0);
    // IMPORTANT: we assert that we can filter here instead of filtering for each branch further
    // down in the tree
    filterNegatedLiterals(cube, activeEvents);

    // insert cube in-place
    // 1. reduce branch
    // IMPORTANT that we do this first to choose the minimal annotation
    // IMPORTANT 2: This loop mitigates against the fact that recursive calls can delete
    // children, potentially invalidating the iterator.
    for (int i = children.size() - 1; i >= 0; i--) {
      if (!children.empty()) {
        children[i]->reduceBranchInternalDown(cube);
      }
    }

    // 2. insert cube
    auto thisChildren = std::move(children);
    children.clear();
    auto newNode = this;
    for (const auto &literal : cube) {  // TODO: refactor, merge with appendBranchInternalDown
      newNode = new Node(newNode, literal);
      newNode->lastUnrollingParent = transitiveClosureNode;
      tableau->unreducedNodes.push(newNode);
    }
    newNode->children = std::move(thisChildren);
    for (const auto &newNodeChild : newNode->children) {
      newNodeChild->parentNode = newNode;
    }
    return;
  }

  // disjunctive
  appendBranchInternalDown(dnfCopy);

  assert(tableau->unreducedNodes.validate());
}

void Node::dnfBuilder(DNF &dnf) const {
  if (isClosed()) {
    return;
  }

  for (const auto &child : children) {
    DNF childDNF;
    child->dnfBuilder(childDNF);
    dnf.insert(dnf.end(), std::make_move_iterator(childDNF.begin()),
               std::make_move_iterator(childDNF.end()));
  }

  if (isLeaf()) {
    dnf.push_back(literal.isNormal() ? Cube{literal} : Cube{});
    return;
  }

  if (!literal.isNormal()) {
    // Ignore non-normal literals.
    return;
  }

  for (auto &cube : dnf) {
    cube.push_back(literal);
  }
}

DNF Node::extractDNF() const {
  DNF dnf;
  dnfBuilder(dnf);
  return dnf;
}

std::optional<DNF> Node::applyRule(const bool modalRule) {
  auto const result = Rules::applyRule(literal, modalRule);
  if (!result) {
    return std::nullopt;
  }

  // to detect at the world cycles
  transitiveClosureNode = Rules::lastRuleWasUnrolling ? this : this->lastUnrollingParent;

  auto disjunction = *result;
  appendBranch(disjunction);

  return disjunction;
}

void Node::inferModal() {
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
    const CanonicalSet e1 = edgeLiteral.leftEvent;
    const CanonicalSet e2 = edgeLiteral.rightEvent;
    const CanonicalRelation b = Relation::newBaseRelation(*edgeLiteral.identifier);
    const CanonicalSet e1b = Set::newSet(SetOperation::image, e1, b);
    const CanonicalSet be2 = Set::newSet(SetOperation::domain, e2, b);

    const CanonicalSet search1 = e1b;
    const CanonicalSet replace1 = e2;
    const CanonicalSet search2 = be2;
    const CanonicalSet replace2 = e1;

    // Compute substituted literals and append.
    Cube subResult = substitute(literal, search1, replace1);
    for (const auto &lit : substitute(literal, search2, replace2)) {
      if (!contains(subResult, lit)) {
        subResult.push_back(lit);
      }
    }
    appendBranch(subResult);
  }
}

void Node::inferModalTop() {
  if (!literal.negated) {
    return;
  }

  // get all events
  const Node *cur = this;
  EventSet existentialReplaceEvents;
  while ((cur = cur->parentNode) != nullptr) {
    const Literal &lit = cur->literal;
    if (lit.negated) {
      continue;
    }

    // Normal and positive literal: collect new events
    const auto &newEvents = lit.events();
    existentialReplaceEvents.insert(newEvents.begin(), newEvents.end());
  }

  const auto &univeralSearchEvents = literal.topEvents();

  for (const auto search : univeralSearchEvents) {
    for (const auto replace : existentialReplaceEvents) {
      // [search] -> {replace}
      // replace all occurrences of the same search at once
      const CanonicalSet searchSet = Set::newTopEvent(search);
      const CanonicalSet replaceSet = Set::newEvent(replace);
      const auto substituted = literal.substituteAll(searchSet, replaceSet);
      if (substituted.has_value()) {
        appendBranch(substituted.value());
      }
    }
  }
}

void Node::inferModalAtomic() {
  // throw std::logic_error("error");
  const Literal &edgeLiteral = literal;
  // (e1, e2) \in b
  assert(edgeLiteral.validate());
  const CanonicalSet e1 = edgeLiteral.leftEvent;
  const CanonicalSet e2 = edgeLiteral.rightEvent;
  const CanonicalRelation b = Relation::newBaseRelation(*edgeLiteral.identifier);
  const CanonicalSet e1b = Set::newSet(SetOperation::image, e1, b);
  const CanonicalSet be2 = Set::newSet(SetOperation::domain, e2, b);

  const CanonicalSet search1 = e1b;
  const CanonicalSet replace1 = e2;
  const CanonicalSet search2 = be2;
  const CanonicalSet replace2 = e1;

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
    // cases for [f] -> {e}
    const auto &univeralSearchEvents = curLit.topEvents();
    for (const auto search : univeralSearchEvents) {
      const CanonicalSet searchSet = Set::newTopEvent(search);

      const auto substituted1 = curLit.substituteAll(searchSet, replace1);
      if (substituted1.has_value()) {
        appendBranch(substituted1.value());
      }

      const auto substituted2 = curLit.substituteAll(searchSet, replace2);
      if (substituted2.has_value()) {
        appendBranch(substituted2.value());
      }
    }
  }
}

void Node::toDotFormat(std::ofstream &output) const {
  // tooltip
  output << "N" << this << "[tooltip=\"";
  output << this << "\n\n";  // address
  output << "--- LITERAL --- \n";
  if (literal.operation == PredicateOperation::setNonEmptiness && literal.negated) {
    output << "annotation: \n";
    output << Annotated::toString<true>(literal.annotatedSet());  // annotation id
    output << "\n\n";
    output << Annotated::toString<false>(literal.annotatedSet());  // annotation base
    output << "\n";
  }
  output << "events: \n";
  output << toString(literal.events()) << "\n";
  output << "normalEvents: \n";
  output << toString(literal.normalEvents()) << "\n";
  output << "lastUnrollingParent: \n";
  output << lastUnrollingParent << "\n";
  output << "--- BRANCH --- \n";
  output << "activeEvents: \n";
  output << toString(activeEvents) << "\n";
  output << "\n";
  output << "activeEventBasePairs: \n";
  output << toString(activeEventBasePairs);

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
