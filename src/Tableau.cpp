#include "Tableau.h"

#include <iostream>
#include <unordered_set>

#include "Assumption.h"
#include "utility.h"

Tableau::Tableau(const Cube &cube) {
  assert(validateCube(cube));
  // avoids the need for multiple root nodes
  auto dummyNode = new Node(nullptr, TOP);
  rootNode = std::unique_ptr<Node>(dummyNode);
  rootNode->tableau = this;

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
    if (currentNode->applyRule()) {
      assert(unreducedNodes.validate());
      assert(currentNode->parentNode->validate());
      removeNode(currentNode);  // in-place rule application
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
      currentNode->inferModalTop();
    }

    if (currentNode->literal.operation == PredicateOperation::setNonEmptiness) {
      // Rule (~aL), Rule (~aR)
      currentNode->inferModal();
    }

    if (currentNode->literal.isPositiveEdgePredicate()) {
      // Rule (~\top_1)
      // e=f, e\in A, (e,f)\in a, e != 0
      // Rule (~aL), Rule (~aR)
      currentNode->inferModalAtomic();
    }

    // 3) Saturation Rules
    if (!Assumption::baseAssumptions.empty()) {
      auto literal = Rules::saturateBase(currentNode->literal);
      if (literal) {
        currentNode->appendBranch(*literal);
      }
    }

    if (!Assumption::idAssumptions.empty()) {
      auto literal = Rules::saturateId(currentNode->literal);
      if (literal) {
        currentNode->appendBranch(*literal);
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
    auto result = currentNode->applyRule(true);
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
  const auto renaming = Renaming(from, to);
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
      unreducedNodes.push(renamedCur);
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

DNF Tableau::dnf() {
  solve();
  auto dnf = rootNode->extractDNF();
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
