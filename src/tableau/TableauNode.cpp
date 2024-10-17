#include "TableauNode.h"

#include <deque>
#include <iostream>
#include <ranges>

#include "../utility.h"
#include "Rules.h"
#include "Tableau.h"

namespace {
// ---------------------- Anonymous helper functions ----------------------

bool isAppendable(const DNF &dnf) { return std::ranges::none_of(dnf, &Cube::empty); }

void reduceDNFAtAWorldCycle(DNF &dnf, const Node *transitiveClosureNode) {
  if (transitiveClosureNode == nullptr || dnf.empty()) {
    return;
  }
  assert(transitiveClosureNode->validate());

  auto [begin, end] = std::ranges::remove_if(dnf, [&](const auto &cube) {
    return std::ranges::any_of(cube, [&](const Literal &cubeLit) {
      assert(cubeLit.validate());
      return !cubeLit.negated && cubeLit == transitiveClosureNode->getLiteral();
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
  Stats::diff("reduceDNF - removed cubes").first(dnf.size());
  auto [begin, end] = std::ranges::remove_if(
      dnf, [&](const auto &cube) { return cubeHasNegatedLiteral(cube, literal); });
  dnf.erase(begin, end);
  Stats::diff("reduceDNF - removed cubes").second(dnf.size());

  // remove l from dnf
  Stats::diff("reduceDNF - removed literals").first(flatten<Literal>(dnf).size());
  for (auto &cube : dnf) {
    auto [begin, end] = std::ranges::remove(cube, literal);
    cube.erase(begin, end);
  }
  Stats::diff("reduceDNF - removed literals").second(flatten<Literal>(dnf).size());

  assert(validateDNF(dnf));
}

Cube substituteAllOnce(const Literal &literal, const CanonicalSet search,
                       const CanonicalSet replace) {
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

// ===========================================================================================
// ====================================== Construction =======================================
// ===========================================================================================

Node::Node(Node *parent, Literal literal)
    : tableau(parent->tableau), parentNode(parent), literal(std::move(literal)) {
  assert(parent != nullptr);
  parent->children.emplace_back(this);
}

Node::Node(Tableau *tableau, Literal literal) : tableau(tableau), literal(std::move(literal)) {
  assert(tableau != nullptr);
}

Node::~Node() {
  // sentinel dummy nodes in worklist have no tableau
  if (tableau != nullptr) {
    // remove this from lastUnrollingParent in xrefMap and this from xrefMap
    tableau->crossReferenceMap.erase(this);
    if (lastUnrollingParent != nullptr &&
        tableau->crossReferenceMap.contains(lastUnrollingParent)) {
      tableau->crossReferenceMap.at(lastUnrollingParent).erase(this);
    }

    // remove this from unreducedNodes
    tableau->unreducedNodes.erase(this);
  }
}

// ===========================================================================================
// ======================================= Validation ========================================
// ===========================================================================================

bool Node::validate() const {
  if (tableau == nullptr) {
    std::cout << "Invalid node(no tableau) " << this << ": " << literal.toString();
    std::flush(std::cout);
    return false;
  }
  if (tableau->getRoot() != this && parentNode == nullptr) {
    std::cout << "Invalid node(no parent) " << this << ": " << literal.toString();
    std::flush(std::cout);
    return false;
  }
  assert(lastUnrollingParent == nullptr ||
         tableau->crossReferenceMap.at(lastUnrollingParent).contains(const_cast<Node *>(this)));
  assert(!tableau->crossReferenceMap.contains(this) ||
         std::ranges::all_of(tableau->crossReferenceMap.at(this), [&](const Node *node) {
           assert(node->lastUnrollingParent == this);
           return node->lastUnrollingParent == this;
         }));

  // crossReferenceMap is acyclic (in connected component of this node)
  auto noIncomingEdge = this;  // interpret crossReferenceMap as outgoing edges
  std::unordered_set<const Node *> visited;
  while (noIncomingEdge->getLastUnrollingParent() != nullptr) {
    const auto &[_, inserted] = visited.insert(noIncomingEdge);
    assert(inserted);  // otherwise there is cycle
    noIncomingEdge = noIncomingEdge->getLastUnrollingParent();
  }

  visited.clear();
  std::deque<const Node *> stack;
  stack.push_back(noIncomingEdge);
  visited.insert(noIncomingEdge);

  while (!stack.empty()) {
    const auto node = stack.back();
    stack.pop_back();

    if (tableau->crossReferenceMap.contains(node)) {                      // has outgoing edges
      for (const auto &nextNode : tableau->crossReferenceMap.at(node)) {  // for each outgoing edge
        const bool cycleFound = std::find(stack.begin(), stack.end(), nextNode) != stack.end();
        assert(!cycleFound);

        const auto &[_, inserted] = visited.insert(nextNode);
        if (inserted) {
          stack.push_back(nextNode);
        }
      }
    }
  }

  return literal.validate();
}

bool Node::validateRecursive() const {
  assert(validate());
  assert(std::ranges::all_of(children, &Node::validateRecursive));
  return true;
}

// ===========================================================================================
// ==================================== Node manipulation ====================================
// ===========================================================================================

void Node::setLastUnrollingParent(const Node *node) {
  if (node == nullptr) {
    lastUnrollingParent = nullptr;
    return;
  }

  if (node != lastUnrollingParent) {
    auto &xrefMap = tableau->crossReferenceMap;
    // remove old reference
    if (lastUnrollingParent != nullptr) {
      assert(xrefMap.contains(lastUnrollingParent));
      auto &lastSet = xrefMap.at(lastUnrollingParent);
      assert(lastSet.contains(this));
      lastSet.erase(this);
    }
    // insert new reference
    xrefMap[node].insert(this);

    assert(lastUnrollingParent == nullptr ||
           !tableau->crossReferenceMap.at(lastUnrollingParent).contains(this));
  }

  // set value
  lastUnrollingParent = node;
}

void Node::attachChild(std::unique_ptr<Node> child) {
  assert(child->parentNode == nullptr && "Trying to attach already attached child.");
  child->parentNode = this;
  children.push_back(std::move(child));
}

void Node::attachChildren(std::vector<std::unique_ptr<Node>> newChildren) {
  std::ranges::for_each(newChildren, [&](auto &child) { attachChild(std::move(child)); });
}

std::unique_ptr<Node> Node::detachChild(Node *child) {
  assert(child->parentNode == this && "Cannot detach parentless child");
  const auto childIt = std::ranges::find(children, child, &std::unique_ptr<Node>::get);
  assert(childIt != children.end() && "Invalid child to detach.");

  auto detachedChild = std::move(*childIt);
  detachedChild->parentNode = nullptr;
  children.erase(childIt);
  return std::move(detachedChild);
}

std::vector<std::unique_ptr<Node>> Node::detachAllChildren() {
  std::ranges::for_each(children, [](auto &child) { child->parentNode = nullptr; });
  auto detachedChildren = std::move(children);
  children.clear();
  return std::move(detachedChildren);
}

void Node::rename(const Renaming &renaming) { literal.rename(renaming); }

// ===========================================================================================
// ==================================== Tree manipulation ====================================
// ===========================================================================================

// deletes literals in dnf that are already in prefix
// if negated literal occurs we omit the whole cube
void Node::appendBranchInternalUp(DNF &dnf) const {
  auto node = this;
  do {
    if (!isAppendable(dnf)) {
      return;
    }
    reduceDNF(dnf, node->literal);
  } while ((node = node->parentNode) != nullptr);
}

void Node::reduceBranchInternalDown(NodeCube &nodeCube) {
  const auto cube = nodeCube | std::views::transform(&Node::literal);

  if (isClosed()) {
    return;
  }

  if (cubeHasNegatedLiteral(cube, literal)) {
    closeBranch();
    return;
  }

  // IMPORTANT: This loop may delete iterated children,
  // so we need to perform a safer kind of iteration
  for (auto childIt = beginSafe(); childIt != endSafe(); ++childIt) {
    childIt->reduceBranchInternalDown(nodeCube);
  }

  const auto nodeIt = std::ranges::find(nodeCube, literal, &Node::literal);
  const bool cubeContainsLiteral = nodeIt != nodeCube.end();
  if (cubeContainsLiteral) {
    Stats::counter("reduceBranch - delete node")++;
    const auto node = *nodeIt;

    // choose minimal annotation
    node->literal.annotation =
        Annotation<SaturationAnnotation>::min(node->literal.annotation, literal.annotation);

    // if this has cross references(aka is a lastUnrollingParent), then use node as alternative
    if (tableau->crossReferenceMap.contains(this)) {
      // IMPORTANT copy to avoid iterating over set while manipulating
      const auto xRefs = tableau->crossReferenceMap.at(this);
      for (const auto nodeWithReference : xRefs) {
        nodeWithReference->setLastUnrollingParent(node);
        assert(!tableau->crossReferenceMap.at(this).contains(nodeWithReference));
      }
    }

    tableau->deleteNode(this);
  }
}

void Node::appendBranchInternalDownDisjunctive(DNF &dnf) {
  assert(validateDNF(dnf));

  if (isClosed()) {
    // Closed leaf: nothing to do
    assert(tableau->unreducedNodes.validate());
    return;
  }

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
    // Copy DNF for all children but the last one (for the last one we can reuse the DNF).
    // TODO: Safe iterate?
    for (const auto &child : std::ranges::drop_view(children, 1)) {
      DNF branchCopy(dnf);
      child->appendBranchInternalDownDisjunctive(branchCopy);  // copy for each branching
    }
    children[0]->appendBranchInternalDownDisjunctive(dnf);
    return;
  }

  // Open leaf
  assert(isLeaf() && !isClosed());
  assert(isAppendable(dnf));

  // append: transform dnf into a tableau and append it
  for (const auto &cube : dnf) {
    auto newNode = this;
    for (const auto &literal : cube) {
      newNode = new Node(newNode, literal);
      newNode->setLastUnrollingParent(transitiveClosureNode);
      tableau->unreducedNodes.push(newNode);
    }
  }
}

void Node::closeBranch() {
  assert(this != tableau->getRoot());
  assert(tableau->unreducedNodes.validate());
  // It is safe to clear the children: the Node destructor
  // will make sure to remove them from worklist
  std::ignore = detachAllChildren();
  assert(tableau->unreducedNodes.validate());  // validate that it was indeed safe to clear
  const auto bottom = new Node(this, BOTTOM);
  bottom->_isClosed = true;

  // update isClosed cache
  auto cur = this;
  do {
    cur->_isClosed = std::ranges::all_of(cur->children, &Node::isClosed);
  } while (cur->isClosed() && (cur = cur->parentNode) != nullptr);
}

// IMPORTANT this method relies on the fact that we consider positive literals first
void Node::removeUselessLiterals(boost::container::flat_set<SetOfSets> &activePairCubes) {
  assert(activePairCubes.size() == 1);
  auto &activePairs = *activePairCubes.begin();
  const auto &literalPairs = literal.eventBasePairs();
  if (!literal.negated) {
    activePairs.insert(literalPairs.begin(), literalPairs.end());
  }

  // IMPORTANT: This loop may delete iterated children,
  // so we need to perform a safer kind of iteration
  if (!isLeaf()) {
    const auto activePairCubesCopy = activePairCubes;
    activePairCubes.clear();
    for (auto childIt = beginSafe(); childIt != endSafe(); ++childIt) {
      auto activePairsCopyTemp = activePairCubesCopy;
      childIt->removeUselessLiterals(activePairsCopyTemp);
      activePairCubes.insert(activePairsCopyTemp.begin(), activePairsCopyTemp.end());
    }
  }

  if (literal.negated && std::ranges::all_of(activePairCubes, [&](const SetOfSets &active) {
        return !isLiteralActive(literal, active, true);
      })) {
    // TODO: do we have to do this while lazy saturation?
    // if (const auto ann = literal.annotation->getValue();
    //     ann->first <= Rules::saturationBound || ann->second <= Rules::saturationBound) {
    //   // if literal can be saturated, saturate first
    //   auto saturatedLiterals = literal.saturate();
    //   appendBranch(saturatedLiterals);
    // }
    Stats::counter("removeUselessLiterals tabl")++;
    tableau->deleteNode(this);
  }
}

void Node::appendBranchInternalDownConjunctive(const DNF &dnf) {
  const auto &cube = dnf.at(0);

  // 1. insert cube in-place
  auto thisChildren = detachAllChildren();
  auto newNode = this;
  NodeCube newNodes;
  for (const auto &literal : cube) {  // TODO: refactor, merge with appendBranchInternalDown
    newNode = new Node(newNode, literal);
    newNode->setLastUnrollingParent(transitiveClosureNode);
    tableau->unreducedNodes.push(newNode);
    newNodes.push_back(newNode);
  }
  newNode->attachChildren(std::move(thisChildren));

  // 2. reduce branch
  // IMPORTANT: This loop mitigates against the fact that recursive calls can delete
  // children, potentially invalidating the iterator.
  // IMPORTANT: This loop may delete iterated children,
  // so we need to perform a safer kind of iteration
  Stats::counter("reduceBranch - delete node").reset();
  for (auto childIt = newNode->beginSafe(); childIt != newNode->endSafe(); ++childIt) {
    childIt->reduceBranchInternalDown(newNodes);
  }
}

void Node::appendBranch(const DNF &dnf) {
  assert(validateDNF(dnf));
  assert(!dnf.empty());     // empty DNF makes no sense
  assert(dnf.size() <= 2);  // We only support binary branching for now (might change in the future)

  if (isClosed()) {
    return;
  }

  DNF dnfCopy(dnf);
  reduceDNFAtAWorldCycle(dnfCopy, transitiveClosureNode);
  appendBranchInternalUp(dnfCopy);

  // empty dnf
  const bool contradiction = dnfCopy.empty();
  if (contradiction) {
    closeBranch();
    return;
  }

  // conjunctive
  const bool isConjunctive = dnfCopy.size() == 1;
  if (isConjunctive) {
    appendBranchInternalDownConjunctive(dnfCopy);
  } else {
    appendBranchInternalDownDisjunctive(dnfCopy);
  }
}

std::optional<DNF> Node::applyRule() {
  auto const result = Rules::applyRule(literal);
  if (!result) {
    return std::nullopt;
  }

  // to detect at the world cycles
  transitiveClosureNode = Rules::lastRuleWasUnrolling ? this : this->lastUnrollingParent;

  auto disjunction = *result;
  appendBranch(disjunction);

  return disjunction;
}

void Node::inferModalUp() {
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
    Cube subResult = substituteAllOnce(literal, search1, replace1);
    for (const auto &lit : substituteAllOnce(literal, search2, replace2)) {
      if (!contains(subResult, lit)) {
        subResult.push_back(lit);
      }
    }
    appendBranch(subResult);
  }
}

void Node::inferModalDown(const Literal &negatedLiteral) {
  if (isClosed()) {
    return;
  }

  for (const auto &child : children) {
    child->inferModalDown(negatedLiteral);
  }

  if (!literal.isPositiveEdgePredicate()) {
    return;
  }

  const CanonicalSet e1 = literal.leftEvent;
  const CanonicalSet e2 = literal.rightEvent;
  const CanonicalRelation b = Relation::newBaseRelation(literal.identifier.value());
  const CanonicalSet e1b = Set::newSet(SetOperation::image, e1, b);
  const CanonicalSet be2 = Set::newSet(SetOperation::domain, e2, b);
  const CanonicalSet search1 = e1b;
  const CanonicalSet replace1 = e2;
  const CanonicalSet search2 = be2;
  const CanonicalSet replace2 = e1;

  // Compute substituted literals and append.
  Cube subResult = substituteAllOnce(negatedLiteral, search1, replace1);
  for (const auto &lit : substituteAllOnce(negatedLiteral, search2, replace2)) {
    if (!contains(subResult, lit)) {
      subResult.push_back(lit);
    }
  }
  appendBranch(subResult);
}

void Node::inferModal() {
  if (literal.negated) {
    inferModalUp();
    inferModalDown(literal);
  }
}

// replace fullSet by concrete positive existential events
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

  const auto search = Set::fullSet();
  for (const auto replace : existentialReplaceEvents) {
    // [T] -> {replace}
    const CanonicalSet replaceSet = Set::newEvent(replace);
    const auto substituted = substituteAllOnce(literal, search, replaceSet);
    appendBranch(substituted);
  }
}

void Node::inferModalBaseSetUp() {
  const Node *cur = this;
  while ((cur = cur->parentNode) != nullptr) {
    if (cur->literal.isPositiveSetPredicate()) {
      // e \in A
      const auto e = cur->literal.leftEvent;
      const auto A = Set::newBaseSet(cur->literal.identifier.value());
      appendBranch(substituteAllOnce(literal, A, e));
    }
  }
}

void Node::inferModalBaseSetDown(const Literal &negatedLiteral) {
  if (isClosed()) {
    return;
  }

  for (const auto &child : children) {
    child->inferModalBaseSetDown(negatedLiteral);
  }

  if (!literal.isPositiveSetPredicate()) {
    return;
  }

  // e \in A
  const auto e = literal.leftEvent;
  const auto A = Set::newBaseSet(literal.identifier.value());
  appendBranch(substituteAllOnce(negatedLiteral, A, e));
}

// assumption: since all positive literals are normalized before we consider negated
// we know that all possible set memberships are known
void Node::inferModalBaseSet() {
  if (literal.negated) {
    inferModalBaseSetUp();
    inferModalBaseSetDown(literal);
  }
}

Cube Node::inferModalAtomicNode(const CanonicalSet search1, const CanonicalSet replace1,
                                const CanonicalSet search2, const CanonicalSet replace2) {
  if (!literal.negated || !literal.isNormal()) {
    return {};
  }
  Cube newLiterals;
  // Negated and normal lit
  // negated modal rule
  auto sub1 = substituteAllOnce(literal, search1, replace1);
  auto sub2 = substituteAllOnce(literal, search2, replace2);
  newLiterals.insert(newLiterals.end(), std::make_move_iterator(sub1.begin()),
                     std::make_move_iterator(sub1.end()));
  newLiterals.insert(newLiterals.end(), std::make_move_iterator(sub2.begin()),
                     std::make_move_iterator(sub2.end()));

  // negated top rule: [f] -> {e}
  const auto search = Set::fullSet();
  const auto substituted1 = substituteAllOnce(literal, search, replace1);
  const auto substituted2 = substituteAllOnce(literal, search, replace2);
  newLiterals.insert(newLiterals.end(), std::make_move_iterator(substituted1.begin()),
                     std::make_move_iterator(substituted1.end()));
  newLiterals.insert(newLiterals.end(), std::make_move_iterator(substituted2.begin()),
                     std::make_move_iterator(substituted2.end()));

  removeDuplicates(newLiterals);  // TODO: performance?
  return newLiterals;
}

void Node::inferModalAtomicUp(const CanonicalSet search1, const CanonicalSet replace1,
                              const CanonicalSet search2, const CanonicalSet replace2) {
  Cube newLiterals;
  auto *cur = this;
  while ((cur = cur->parentNode) != nullptr) {
    auto newLiteralsNode = cur->inferModalAtomicNode(search1, replace1, search2, replace2);
    for (const auto &newLit : newLiteralsNode) {
      if (!contains(newLiterals, newLit)) {
        newLiterals.push_back(newLit);
      }
    }
  }
  appendBranch(newLiterals);
}

void Node::inferModalAtomicDown(const CanonicalSet search1, const CanonicalSet replace1,
                                const CanonicalSet search2, const CanonicalSet replace2) {
  if (isClosed()) {
    return;
  }

  for (const auto &child : children) {
    child->inferModalAtomicDown(search1, replace1, search2, replace2);
  }

  const auto newLiteralsNode = inferModalAtomicNode(search1, replace1, search2, replace2);
  appendBranch(newLiteralsNode);
}

void Node::inferModalAtomic() {
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

  inferModalAtomicUp(search1, replace1, search2, replace2);
  inferModalAtomicDown(search1, replace1, search2, replace2);
}

// ===========================================================================================
// ========================================= Printing ========================================
// ===========================================================================================

void Node::toDotFormat(std::ofstream &output) const {
  // tooltip
  output << "N" << this << "[tooltip=\"";
  output << this << "\n\n";  // address
  output << "unreduced: " << tableau->unreducedNodes.contains(this) << "\n";
  if (literal.operation == PredicateOperation::setNonEmptiness && literal.negated) {
    output << "Id annotation: \n";
    output << Annotated::toString<true>(literal.annotatedSet()) << "\n";  // annotation id
    output << "base annotation: \n";
    output << Annotated::toString<false>(literal.annotatedSet());  // annotation base
    output << "\n";
  }
  output << "events: " << toString(literal.events()) << "\n";
  output << "normalEvents: " << toString(literal.normalEvents()) << "\n";
  output << "saturatedEventPairs: ";
  for (const auto pair : literal.saturatedEventBasePairs()) {
    output << pair->toString() << " ";
  }
  output << "\n";
  output << "lastUnrollingParent: " << lastUnrollingParent << "\n";
  if (tableau->crossReferenceMap.contains(this)) {
    output << "crossrefs: ";
    for (const auto ref : tableau->crossReferenceMap.at(this)) {
      output << ref << " ";
    }
    output << "\n";
  }
  output << "branch equalities: ";
  for (const auto &equality : equalities) {
    output << equality.toString();
  }
  output << "\n";

  // label
  output << "\",label=\"" << literal.toString() << "\"";

  // color closed branches
  if (isClosed()) {
    output << ", fontcolor=green";
  } else if (tableau->unreducedNodes.contains(this)) {
    output << ", fontcolor=blue";
  }
  output << "];\n";

  // children
  for (const auto &child : children) {
    child->toDotFormat(output);
    output << "N" << this << " -- "
           << "N" << child << ";\n";
  }
}
