#include "Tableau.h"

#include <iostream>
#include <ranges>
#include <unordered_set>

#include "../Assumption.h"
#include "../utility.h"
#include "Rules.h"

// ===========================================================================================
// ====================================== Construction =======================================
// ===========================================================================================

Tableau::Tableau(const Cube &cube) {
  assert(validateCube(cube));
  // avoids the need for multiple root nodes
  const auto dummyNode = new Node(this, TOP);
  rootNode = std::unique_ptr<Node>(dummyNode);

  Node *parentNode = dummyNode;
  for (const auto &literal : cube) {
    auto *newNode = new Node(parentNode, literal);
    assert(newNode->validate());
    unreducedNodes.push(newNode);
    parentNode = newNode;
  }
}

// ===========================================================================================
// ======================================= Validation ========================================
// ===========================================================================================

bool Tableau::validate() const {
  assert(rootNode->validateRecursive());
  assert(unreducedNodes.validate());
  return true;
}

// ===========================================================================================
// ================================== Tableau computation ====================================
// ===========================================================================================

void Tableau::deleteNode(Node *node) {
  assert(validate());
  assert(node != rootNode.get());            // node is not root node
  assert(node->getParentNode() != nullptr);  // there is always a dummy root node
  assert(node->getParentNode()->validateRecursive());
  assert(!crossReferenceMap.contains(node) || crossReferenceMap.at(node).empty());  // has no xrefs

  const auto parentNode = node->getParentNode();
  if (node->isLeaf()) {
    // SUPER IMPORTANT OPTIMIZATION: If we remove a leaf node, we can remove all children from
    // that leaf's parent. The reason is as follows:
    // Consider the branch "root ->* parent -> leaf" which just becomes "root ->* parent" after
    // deletion. This new branch subsumes/dominates all branches of the form "root ->* parent ->+
    // ..." so we can get rid of all those branches. We do so by deleting all children from the
    // parent node.
    std::ignore = parentNode->detachAllChildren();
  } else {
    // No leaf: move all children to parent's children
    parentNode->attachChildren(node->detachAllChildren());
    // Remove node from parent. This will automatically delete the node and remove it from the
    // worklist.
    std::ignore = node->detachFromParent();
  }

  assert(parentNode->validateRecursive());
  assert(unreducedNodes.validate());
  assert(validate());
}

void Tableau::normalize() {
  while (!unreducedNodes.isEmpty()) {
    exportDebug("debug");

    Node *currentNode = unreducedNodes.pop();
    assert(currentNode->validate());
    assert(currentNode->getParentNode()->validate());
    assert(validate());
    if (currentNode->isClosed()) {
      continue;
    }
    exportDebug("debug");

    Node::transitiveClosureNode = currentNode->getLastUnrollingParent();

    // 1) Rules that just rewrite a single literal
    if (currentNode->applyRule()) {
      assert(unreducedNodes.validate());
      assert(currentNode->getParentNode()->validate());
      if (!Rules::lastRuleWasUnrolling) {
        deleteNode(currentNode);  // in-place rule application
      }
      continue;
    }

    // 2) Renaming rule
    if (currentNode->getLiteral().isPositiveEqualityPredicate()) {
      // we want to process equalities first
      // Rule (\equivL), Rule (\equivR)
      renameBranches(currentNode);
      continue;
    }

    assert(currentNode->getLiteral().isNormal());

    // 2) Rules which require context (only to normalized literals)
    // IMPORTANT: it is not sufficient to look upwards

    if (currentNode->getLiteral().hasFullSet() /* TODO (topEvent optimization): hasTopEvent()*/) {
      // Rule (~\top_1)
      // here: we replace universal events by concrete positive existential events
      assert(currentNode->getLiteral().negated);
      currentNode->inferModalTop();
    }

    if (currentNode->getLiteral().operation == PredicateOperation::setNonEmptiness) {
      // Rule (~aL), Rule (~aR)
      currentNode->inferModal();
    }

    if (currentNode->getLiteral().isPositiveEdgePredicate()) {
      // Rule (~\top_1)
      // e=f, e\in A, (e,f)\in a, e != 0
      // Rule (~aL), Rule (~aR)
      currentNode->inferModalAtomic();
    }

    // 3) Saturation Rules
    if (!Assumption::baseAssumptions.empty()) {
      if (auto literal = Rules::saturateBase(currentNode->getLiteral())) {
        currentNode->appendBranch(*literal);
      }
    }

    if (!Assumption::idAssumptions.empty()) {
      if (auto literal = Rules::saturateId(currentNode->getLiteral())) {
        currentNode->appendBranch(*literal);
      }
    }
  }
}

bool Tableau::tryApplyModalRuleOnce(int applyToEvent) {
  while (!unreducedNodes.isEmpty()) {
    Node *currentNode = unreducedNodes.pop();

    if (const auto result =
            Rules::applyPositiveModalRule(currentNode->getLiteral(), applyToEvent)) {
      assert(unreducedNodes.validate());
      assert(currentNode->getParentNode()->validate());

      // to detect at the world cycles
      // set it before appendBranch call
      currentNode->transitiveClosureNode = currentNode->getLastUnrollingParent();
      currentNode->appendBranch(result.value());

      deleteNode(currentNode);
      return true;
    }
  }

  return false;
}

// ===========================================================================================
// ======================================= Renaming ==========================================
// ===========================================================================================

Node *Tableau::renameBranchesInternalUp(Node *lastSharedNode, const int from, const int to,
                                        std::unordered_set<Literal> &allRenamedLiterals,
                                        std::unordered_map<const Node *, Node *> &originalToCopy) {
  const Renaming renaming = Renaming::simple(from, to);

  // Determine first node (closest to root) that has to be renamed.
  // Everything above is unaffected and thus we can share the common prefix for the renamed
  // branch.
  const Node *firstToRename = nullptr;
  const Node *cur = lastSharedNode;
  while (cur != nullptr) {
    if (cur->getLiteral().events().contains(from)) {
      firstToRename = cur;
    }
    cur = cur->getParentNode();
  }

  // Nothing to rename: keep branch as is.
  if (firstToRename == nullptr) {
    return lastSharedNode;
  }
  const auto commonPrefix = firstToRename->getParentNode();

  // Copy & rename from lastSharedNode to firstToRename.
  Node *renamedNodeCopy = nullptr;
  std::unique_ptr<Node> copiedBranch = nullptr;
  cur = lastSharedNode;
  while (cur != commonPrefix) {
    assert(cur->validate());
    auto litCopy = Literal(cur->getLiteral());
    litCopy.rename(renaming);

    // Check for duplicates & create renamed node only if new.
    auto [_, isNew] = allRenamedLiterals.insert(litCopy);
    if (isNew) {
      auto renamedCur = new Node(cur->getTableau(), litCopy);
      originalToCopy.insert({cur, renamedCur});
      renamedCur->setLastUnrollingParent(cur->getLastUnrollingParent());
      if (copiedBranch != nullptr) {
        renamedCur->attachChild(std::move(copiedBranch));
      } else {
        renamedNodeCopy = renamedCur;
      }
      // if cur is in unreduced nodes push renamedCur
      // TODO: check if this is sound
      // if (unreducedNodes.contains(cur)) {
      //   unreducedNodes.push(renamedCur);
      // }
      unreducedNodes.push(renamedCur);
      copiedBranch = std::unique_ptr<Node>(renamedCur);
    }

    cur = cur->getParentNode();
  }
  // update meta data based on originalToCopy
  Node *curCopy = originalToCopy.at(lastSharedNode);
  assert(curCopy->isLeaf());
  while (curCopy != nullptr) {
    if (originalToCopy.contains(curCopy->getLastUnrollingParent())) {
      curCopy->setLastUnrollingParent(originalToCopy.at(curCopy->getLastUnrollingParent()));
    }
    curCopy = curCopy->getParentNode();
  }

  commonPrefix->attachChild(std::move(copiedBranch));
  return renamedNodeCopy;
}

void Tableau::renameBranchesInternalDown(
    Node *node, const Renaming &renaming, std::unordered_set<Literal> &allRenamedLiterals,
    const std::unordered_map<const Node *, Node *> &originalToCopy,
    std::unordered_set<const Node *> &unrollingParents) {
  node->rename(renaming);
  // gather all unrollingParents in the subtree to prevent deleting them
  if (node->getLastUnrollingParent() != nullptr) {
    unrollingParents.insert(node->getLastUnrollingParent());

    if (originalToCopy.contains(node->getLastUnrollingParent())) {
      node->setLastUnrollingParent(originalToCopy.at(node->getLastUnrollingParent()));
    }
  }

  auto [_, isNew] = allRenamedLiterals.insert(node->getLiteral());

  if (!node->isLeaf()) {
    // No leaf: descend recursively
    // Copy allRenamedLiterals for all children but the last one
    for (auto childIt = node->beginSafe(); childIt != node->endSafe(); ++childIt) {
      std::unordered_set<Literal> allRenamedLiteralsCopy;
      if (childIt.isLast()) {
        allRenamedLiteralsCopy = std::move(allRenamedLiterals);
      } else {
        allRenamedLiteralsCopy = allRenamedLiterals;
      }
      renameBranchesInternalDown(*childIt, renaming, allRenamedLiteralsCopy, originalToCopy,
                                 unrollingParents);  // copy for each branching
    }
  }
  if (!isNew && !unrollingParents.contains(node)) {
    deleteNode(node);
  }
  assert(unreducedNodes.validate());
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
void Tableau::renameBranches(Node *node) {
  assert(validate());
  assert(node->getLiteral().operation == PredicateOperation::equality);

  // Compute renaming according to equality predicate (from larger label to lower label).
  const int e1 = node->getLiteral().leftEvent->label.value();
  const int e2 = node->getLiteral().rightEvent->label.value();
  const int from = (e1 < e2) ? e2 : e1;
  const int to = (e1 < e2) ? e1 : e2;
  const auto renaming = Renaming::simple(from, to);
  assert(from != to);

  // Determine first node that belongs to the renamed branches only
  Node *firstUnsharedNode = node;
  while (firstUnsharedNode->getParentNode() != rootNode.get() &&
         firstUnsharedNode->getParentNode()->getChildren().size() <= 1) {
    firstUnsharedNode = firstUnsharedNode->getParentNode();
  }
  const auto lastSharedNode = firstUnsharedNode->getParentNode();

  std::unordered_set<Literal> renamedLiterals;  // To remove identical (after renaming) literals
  std::unordered_map<const Node *, Node *> originalToCopy;
  std::unordered_set<const Node *> unrollingParents;

  const auto renamedLastSharedNode =
      renameBranchesInternalUp(lastSharedNode, from, to, renamedLiterals, originalToCopy);
  renameBranchesInternalDown(firstUnsharedNode, renaming, renamedLiterals, originalToCopy,
                             unrollingParents);

  renamedLastSharedNode->attachChild(firstUnsharedNode->detachFromParent());
}

// ===========================================================================================
// ==================================== DNF computation ======================================
// ===========================================================================================

bool isSubsumed(const Cube &a, const Cube &b) {
  if (a.size() < b.size()) {
    return false;
  }
  return std::ranges::all_of(b, [&](auto const lit) { return contains(a, lit); });
}

void dnfBuilder(const Node *node, DNF &dnf) {
  if (node->isClosed()) {
    return;
  }

  for (const auto &child : node->getChildren()) {
    DNF childDNF;
    dnfBuilder(child.get(), childDNF);
    dnf.insert(dnf.end(), std::make_move_iterator(childDNF.begin()),
               std::make_move_iterator(childDNF.end()));
  }

  const auto &literal = node->getLiteral();
  if (node->isLeaf()) {
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

DNF extractDNF(const Node *root) {
  DNF dnf;
  dnfBuilder(root, dnf);
  for (auto &cube : dnf) {
    filterNegatedLiterals(cube);
  }
  return dnf;
}

DNF simplifyDnf(const DNF &dnf) {
  // return dnf;  // To disable simplification
  DNF sortedDnf = dnf;
  std::ranges::sort(sortedDnf, std::less(), &Cube::size);

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

DNF Tableau::computeDnf() {
  assert(validate());
  normalize();
  auto dnf = simplifyDnf(extractDNF(rootNode.get()));
  exportDebug("debug");
  assert(validateDNF(dnf));
  assert(validate());
  assert(unreducedNodes.isEmpty());
  return dnf;
}

// ===========================================================================================
// ======================================== Printing =========================================
// ===========================================================================================

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
