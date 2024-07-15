#include "Tableau.h"

#include <iostream>
#include <ranges>
#include <unordered_set>

#include "Assumption.h"
#include "Rules.h"
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

Node::~Node() {
  // remove this from unreducedNodes
  tableau->unreducedNodes.erase(this);
}

bool Node::validate() const {
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

Tableau::Tableau(const Cube &cube) {
  assert(validateCube(cube));
  // avoids the need for multiple root nodes
  auto dummyNode = new Node(nullptr, TOP);
  dummyNode->tableau = this;
  rootNode = std::unique_ptr<Node>(dummyNode);

  Node *parentNode = dummyNode;
  for (const auto &literal : cube) {
    auto *newNode = new Node(parentNode, literal);
    parentNode->children.emplace_back(newNode);
    assert(newNode->validate());
    unreducedNodes.push(newNode);
    parentNode = newNode;
  }
}

bool Tableau::validate() const {
  return rootNode->validateRecursive() && unreducedNodes.validate();
}

void Tableau::removeNode(Node *node) {
  assert(validate());
  assert(node != rootNode.get());       // node is not root node
  assert(node->parentNode != nullptr);  // there is always a dummy root node
  assert(node->parentNode->validate());
  const auto parentNode = node->parentNode;

  // SUPER IMPORTANT OPTIMIZATION: If we remove a leaf node, we can remove all children from
  // that leaf's parent. The reason is as follows:
  // Consider the branch "root ->* parent -> leaf" which just becomes "root ->* parent" after
  // deletion. This new branch subsumes/dominates all branches of the form "root ->* parent ->+ ..."
  // so we can get rid of all those branches. We do so by deleting all children from the parent
  // node.
  if (node->isLeaf()) {
    parentNode->children.clear();
    return;
  }

  // No leaf: move all children to parent's children
  for (const auto &child : node->children) {
    child->parentNode = parentNode;
  }
  parentNode->children.insert(parentNode->children.end(),
                              std::make_move_iterator(node->children.begin()),
                              std::make_move_iterator(node->children.end()));

  // Remove node from parent. This will automatically delete the node and remove it from the
  // worklist.
  auto [begin, end] = std::ranges::remove_if(parentNode->children,
                                             [&](auto &element) { return element.get() == node; });
  parentNode->children.erase(begin, end);

  assert(parentNode->validate());
  assert(std::ranges::none_of(parentNode->children,
                              [](const auto &child) { return !child->validate(); }));
  assert(unreducedNodes.validate());
  assert(validate());
}

bool Tableau::solve(int bound) {
  while (!unreducedNodes.isEmpty() && (bound > 0 || bound == -1)) {
    if (bound > 0) {
      bound--;
    }

    Node *currentNode = unreducedNodes.pop();
    assert(currentNode->validate());
    assert(currentNode->parentNode->validate());
    assert(validate());
    exportDebug("debug");

    // 1) Rules that just rewrite a single literal
    if (applyRule(currentNode)) {
      assert(unreducedNodes.validate());
      const bool hasParent =
          currentNode->parentNode != nullptr;  // this happens if branchs has been closed
      if (!Rules::lastRuleWasUnrolling && hasParent) {
        removeNode(currentNode);  // in-place rule application
      }
      continue;
    }

    // 2) Renaming rule
    if (currentNode->literal.isPositiveEqualityPredicate()) {
      // we want to process equalities first
      // currently we assume that there is at most one euqality predicate which is a leaf
      // we could generalize this
      assert(currentNode->isLeaf());

      // Rule (\equivL), Rule (\equivR)
      renameBranch(currentNode);
      continue;
    }

    assert(currentNode->literal.isNormal());

    // 2) Rules which require context (only to normalized literals)
    // if you need two premises l1, l2 and comes after l1 in the branch then the rule is applied if
    // we consider l2
    // we cannot consider edge predicates first and check for premise literals upwards because the
    // conclusion could be such a predicate again
    // TODO: maybe consider lazy T evalutaion (consider T as event)

    if (currentNode->literal.hasTopEvent()) {
      // Rule (~\top_1)
      // here: we replace universal events by concrete positive existential events
      assert(currentNode->literal.negated);
      inferModalTop(currentNode);
    }

    if (currentNode->literal.operation == PredicateOperation::setNonEmptiness) {
      // Rule (~aL), Rule (~aR)
      inferModal(currentNode);
    }

    if (currentNode->literal.isPositiveEdgePredicate()) {
      // Rule (~\top_1)
      // e=f, e\in A, (e,f)\in a, e != 0
      // Rule (~aL), Rule (~aR)
      inferModalAtomic(currentNode);
    }

    // 3) Saturation Rules
    if (!Assumption::baseAssumptions.empty()) {
      auto literal = Rules::saturateBase(currentNode->literal);
      if (literal) {
        appendBranch(currentNode, literal.value());
      }
    }

    if (!Assumption::idAssumptions.empty()) {
      auto literal = Rules::saturateId(currentNode->literal);
      if (literal) {
        appendBranch(currentNode, literal.value());
      }
    }
  }

  // warning if bound is reached
  if (bound == 0 && !unreducedNodes.isEmpty()) {
    std::cout << "[Warning] Configured bound is reached. Answer is imprecise." << std::endl;
  }

  return rootNode->isClosed();
}

bool Tableau::applyRuleA() {
  while (!unreducedNodes.isEmpty()) {
    Node *currentNode = unreducedNodes.pop();

    exportDebug("debug");
    auto result = applyRule(currentNode, true);
    if (!result) {
      continue;
    }
    assert(unreducedNodes.validate());
    assert(currentNode->parentNode->validate());
    removeNode(currentNode);

    // find atomic
    for (const auto &cube : *result) {
      // should be only one cube
      if (std::ranges::any_of(cube, &Literal::isPositiveEdgePredicate)) {
        return true;
      }
    }
    // TODO: clear worklist and add only result of modal rule to it
  }

  return false;
}

/*
 *  Given a leaf node with an equality predicate, renames the branch according to the equality.
 *  The original branch is destroyed and the renamed branch is added to the tableau.
 *  The renamed branch will contain a trivial leaf of the shape "l = l" which is likely
 *  to get removed in the next step.
 *  All newly added nodes are automatically added to the worklist.
 *
 *  NOTE: If different literals are renamed to identical literals, only a single copy is kept.
 */
void Tableau::renameBranch(const Node *leaf) {
  assert(validate());
  assert(leaf->isLeaf());
  assert(leaf->literal.operation == PredicateOperation::equality);

  // Compute renaming according to equality predicate (from larger label to lower label).
  const int e1 = leaf->literal.leftEvent->label.value();
  const int e2 = leaf->literal.rightEvent->label.value();
  const int from = (e1 < e2) ? e2 : e1;
  const int to = (e1 < e2) ? e1 : e2;
  const auto renaming = Renaming::simple(from, to);
  assert(from != to);

  // Determine first node (closest to root) that has to be renamed.
  // Everything above is unaffected and thus we can share the common prefix for the renamed branch.
  const Node *firstToRename = nullptr;
  const Node *cur = leaf;
  while ((cur = cur->parentNode) != nullptr) {
    if (cur->literal.events().contains(from)) {
      firstToRename = cur;
    }
  }

  // Nothing to rename: keep branch as is.
  if (firstToRename == nullptr) {
    return;
  }
  const auto commonPrefix = firstToRename->parentNode;

  // Copy & rename branch.
  std::unordered_set<Literal> allRenamedLiterals;  // To remove identical (after renaming) literals
  bool currentNodeIsShared = false;                // Unshared nodes can be dropped after renaming.
  std::unique_ptr<Node> copiedBranch = nullptr;
  cur = leaf;
  while (cur != commonPrefix) {
    // Copy & rename literal
    assert(cur->validate());
    auto litCopy = Literal(cur->literal);
    litCopy.rename(renaming);

    // Check for duplicates & create renamed node only if new.
    auto [_, isNew] = allRenamedLiterals.insert(litCopy);
    if (isNew) {
      auto renamedCur = new Node(nullptr, litCopy);
      renamedCur->tableau = cur->tableau;
      if (copiedBranch != nullptr) {
        copiedBranch->parentNode = renamedCur;
        renamedCur->children.push_back(std::move(copiedBranch));
      }
      // if cur is in unreduced nodes push renamedCur
      if (cur == leaf || unreducedNodes.contains(cur)) {
        unreducedNodes.push(renamedCur);
      }
      copiedBranch = std::unique_ptr<Node>(renamedCur);
    }

    // Check if we go from unshared nodes to shared nodes: if so, we can delete all unshared nodes
    const auto parentNode = cur->parentNode;
    const auto parentIsShared =
        currentNodeIsShared || (parentNode == commonPrefix || parentNode->children.size() > 1);
    if (!currentNodeIsShared && parentIsShared) {
      // Delete unshared nodes
      auto curIt = std::ranges::find_if(parentNode->children,
                                        [&](auto &child) { return child.get() == cur; });
      parentNode->children.erase(curIt);
      currentNodeIsShared = true;
    }

    cur = parentNode;
  }

  // Append copied branch
  assert(copiedBranch != nullptr);
  copiedBranch->parentNode = commonPrefix;
  commonPrefix->children.push_back(std::move(copiedBranch));
}

void Tableau::appendBranch(Node *node, const DNF &dnf) {
  assert(unreducedNodes.validate());
  assert(validateDNF(dnf));
  assert(!dnf.empty());     // empty DNF makes no sense
  assert(dnf.size() <= 2);  // We only support binary branching for now (might change in the future)

  DNF dnfCopy(dnf);
  appendBranchInternalUp(node, dnfCopy);
  appendBranchInternalDown(node, dnfCopy);

  assert(unreducedNodes.validate());
}

// deletes literals in dnf that are already in prefix
// if negated literal occurs we omit the whole cube
void Tableau::appendBranchInternalUp(const Node *node, DNF &dnf) {
  do {
    assert(validateDNF(dnf));
    if (!isAppendable(dnf)) {
      return;
    }
    reduceDNF(dnf, node->literal);
  } while ((node = node->parentNode) != nullptr);
}

void Tableau::appendBranchInternalDown(Node *node, DNF &dnf) {
  assert(unreducedNodes.validate());
  assert(validateDNF(dnf));
  reduceDNF(dnf, node->literal);

  const bool contradiction = dnf.empty();
  if (contradiction) {
    closeBranch(node);
    assert(unreducedNodes.validate());
    return;
  }
  if (!isAppendable(dnf)) {
    assert(unreducedNodes.validate());
    return;
  }

  if (!node->isLeaf()) {
    // No leaf: descend recursively
    // Copy DNF for all children but the last one (for the last one we can reuse the DNF).
    for (const auto &child : std::ranges::drop_view(node->children, 1)) {
      DNF branchCopy(dnf);
      appendBranchInternalDown(child.get(), branchCopy);  // copy for each branching
    }
    appendBranchInternalDown(node->children[0].get(), dnf);
    assert(unreducedNodes.validate());
    return;
  }

  if (node->isClosed()) {
    // Closed leaf: nothing to do
    assert(unreducedNodes.validate());
    return;
  }

  // Open leaf
  assert(node->isLeaf() && !node->isClosed());
  assert(isAppendable(dnf));

  // filter non-active negated literals
  for (auto &cube : dnf) {
    filterNegatedLiterals(cube, node->activeEvents);
    // TODO: labelBase optimization
    // filterNegatedLiterals(cube, activeEventBasePairs);
  }
  if (!isAppendable(dnf)) {
    return;
  }
  assert(isAppendable(dnf));

  // append: transform dnf into a tableau and append it
  for (const auto &cube : dnf) {
    Node *newNode = node;
    for (const auto &literal : cube) {
      newNode = new Node(newNode, literal);
      newNode->parentNode->children.emplace_back(newNode);
      unreducedNodes.push(newNode);
    }
  }
  assert(unreducedNodes.validate());
}

// removes subtree form node
void Tableau::closeBranch(Node *node) {
  assert(node != rootNode.get());
  assert(unreducedNodes.validate());
  // remove branch by deleting respective child of first branching parent
  Node *deletedChild = node;
  while (deletedChild->parentNode != rootNode.get() &&
         deletedChild->parentNode->children.size() <= 1) {
    deletedChild = deletedChild->parentNode;
  }
  const auto firstBranchingParent = deletedChild->parentNode;

  // It is safe to remove deletedChild from children:
  // Node destructor will make sure to remove subtree from worklist
  auto [begin, end] =
      std::ranges::remove_if(firstBranchingParent->children,
                             [&](const auto &child) { return child.get() == deletedChild; });
  firstBranchingParent->children.erase(begin, end);
  assert(unreducedNodes.validate());  // validate that it was indeed safe to clear

  if (firstBranchingParent == rootNode.get() && rootNode->children.empty()) {
    // single closed branch
    rootNode->children.emplace_back(new Node(node, BOTTOM));
  }
}

std::optional<DNF> Tableau::applyRule(Node *node, const bool modalRule) {
  auto const result = Rules::applyRule(node->literal, modalRule);
  if (!result) {
    return std::nullopt;
  }

  auto disjunction = *result;
  appendBranch(node, disjunction);

  return disjunction;
}

void Tableau::inferModal(Node *node) {
  if (!node->literal.negated) {
    return;
  }

  // Traverse bottom-up
  const Node *cur = node;
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

    appendBranch(node, substitute(node->literal, search1, replace1));
    appendBranch(node, substitute(node->literal, search2, replace2));
  }
}

void Tableau::inferModalTop(Node *node) {
  if (!node->literal.negated) {
    return;
  }

  // get all events
  const Node *cur = node;
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

  const auto &univeralSearchEvents = node->literal.topEvents();

  for (const auto search : univeralSearchEvents) {
    for (const auto replace : existentialReplaceEvents) {
      // [search] -> {replace}
      // replace all occurrences of the same search at once
      const CanonicalSet searchSet = Set::newTopEvent(search);
      const CanonicalSet replaceSet = Set::newEvent(replace);
      const auto substituted = node->literal.substituteAll(searchSet, replaceSet);
      if (substituted.has_value()) {
        appendBranch(node, substituted.value());
      }
    }
  }
}

void Tableau::inferModalAtomic(Node *node) {
  const Literal &edgeLiteral = node->literal;
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

  const Node *cur = node;
  while ((cur = cur->parentNode) != nullptr) {
    exportDebug("debug");
    const Literal &curLit = cur->literal;
    if (!curLit.negated || !curLit.isNormal()) {
      continue;
    }

    // Negated and normal lit
    // check if inside literal can be something inferred
    for (auto &lit : substitute(curLit, search1, replace1)) {
      appendBranch(node, lit);
    }
    for (auto &lit : substitute(curLit, search2, replace2)) {
      appendBranch(node, lit);
    }
    // cases for [f] -> {e}
    const auto &univeralSearchEvents = curLit.topEvents();
    for (const auto search : univeralSearchEvents) {
      const CanonicalSet searchSet = Set::newTopEvent(search);

      const auto substituted1 = curLit.substituteAll(searchSet, replace1);
      if (substituted1.has_value()) {
        appendBranch(node, substituted1.value());
      }

      const auto substituted2 = curLit.substituteAll(searchSet, replace2);
      if (substituted2.has_value()) {
        appendBranch(node, substituted2.value());
      }
    }
  }
}

// FIXME: use or remove
void Tableau::replaceNegatedTopOnBranch(Node *node, const std::vector<int> &events) {
  const Node *cur = node;
  while ((cur = cur->parentNode) != nullptr) {
    const Literal &curLit = cur->literal;
    if (!curLit.negated || !curLit.isNormal()) {
      continue;
    }
    // replace T -> e
    const CanonicalSet top = Set::fullSet();
    for (const auto label : events) {
      const CanonicalSet e = Set::newEvent(label);
      for (auto &lit : substitute(curLit, top, e)) {
        appendBranch(node, lit);
      }
    }
  }
}

bool isSubsumed(const Cube &a, const Cube &b) {
  if (a.size() < b.size()) {
    return false;
  }
  return std::ranges::all_of(b, [&](auto const lit) { return contains(a, lit); });
}

DNF simplifyDnf(const DNF &dnf) {
  // return dnf;  // To disable simplification
  DNF sortedDnf = dnf;
  std::ranges::sort(sortedDnf, std::less<int>(), &Cube::size);

  DNF simplified;
  simplified.reserve(sortedDnf.size());
  for (const auto &c1 : sortedDnf) {
    const bool subsumed =
        std::ranges::any_of(simplified, [&](const auto &c2) { return isSubsumed(c1, c2); });
    if (!subsumed) {
      simplified.push_back(c1);
    }
  }
  /*if (simplified.size() < dnf.size()) {
    std::cout << "DNF reduction: " << dnf.size() << " -> " << simplified.size() << "\n";
  }*/
  return simplified;
}

DNF Tableau::dnf() {
  solve();
  auto dnf = simplifyDnf(rootNode->extractDNF());
  assert(validateDNF(dnf));
  return dnf;
}

void Tableau::toDotFormat(std::ofstream &output) const {
  output << "graph {" << std::endl << "node[shape=\"plaintext\"]" << std::endl;
  if (rootNode != nullptr) {
    rootNode->toDotFormat(output);
  }
  output << "}" << std::endl;
}

void Tableau::exportProof(const std::string &filename) const {
  std::ofstream file("./output/" + filename + ".dot");
  toDotFormat(file);
  file.close();
}

void Tableau::exportDebug(const std::string &filename) const {
#if (DEBUG)
  exportProof(filename);
#endif
}
