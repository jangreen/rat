#include "Tableau.h"

#include <iostream>
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

DNF Tableau::extractDNF() const {
  DNF dnf;
  dnfBuilder(rootNode.get(), dnf);
  return dnf;
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
  assert(node != rootNode.get());            // node is not root node
  assert(node->getParentNode() != nullptr);  // there is always a dummy root node
  assert(!crossReferenceMap.contains(node) || crossReferenceMap.at(node).empty());  // has no xrefs

  if (unreducedNodes.contains(node)) {
    unreducedNodes.erase(node);
  }

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
}

// size_t lastSize = 0;
// std::string lastString;
void Tableau::normalize() {
  Stats::counter("#iterations - normalize").reset();

  bool activePairsPopulated = false;
  while (!unreducedNodes.isEmpty()) {
    // remove useless literals in each iteration
    // do this after all positive literals are processed
    // unreduced nodes could become empty
    const auto allPositiveLiteralsProcessed = unreducedNodes.top()->getLiteral().negated;
    if (allPositiveLiteralsProcessed && !activePairsPopulated) {
      SetOfSets activePairs = {};
      rootNode->computeActivePairs(activePairs);
      exportDebug("debug-tableau");
      activePairsPopulated = true;
    }
    // TODO: remove
    // if (modCounter % 50 == 0) {
    //   if (allPositiveLiteralsProcessed) {
    //     removeUselessLiterals();
    //     if (unreducedNodes.isEmpty()) {
    //       break;
    //     }
    //   }
    // }
    // modCounter++;

    Stats::counter("#iterations - normalize")++;
    Node *currentNode = unreducedNodes.pop();
    exportDebug("debug-tableau");
    assert(currentNode->validate());
    assert(currentNode->getParentNode()->validate());
    if (currentNode->isClosed()) {
      continue;
    }

    Node::transitiveClosureNode = currentNode->getLastUnrollingParent();

    // if (size(rootNode.get()) > lastSize * 1.5 && lastSize != 0) {
    //   std::cout << lastString << "\n" << lastSize << std::endl;
    //
    //   std::cout << currentNode->getLiteral().toString() << "\n"
    //             << size(rootNode.get()) << ", " << leafSize(rootNode.get()) << ", "
    //             << depth(rootNode.get()) << std::endl;
    // }
    // lastSize = size(rootNode.get());
    // lastString = currentNode->getLiteral().toString();

    // 1) Rules that just rewrite a single literal
    if (currentNode->applyRule()) {
      if (!Rules::lastRuleWasUnrolling) {  // prevent cycling
        deleteNode(currentNode);           // in-place rule application
      }
      continue;
    }

    // 2) Renaming rule
    if (currentNode->getLiteral().isPositiveEqualityPredicate()) {
      // Rule (\equivL), Rule (\equivR)
      renameBranches(currentNode);
      continue;
    }

    if (currentNode->getLiteral().hasBaseSet()) {
      // Rule ~(A)
      currentNode->inferModalBaseSet();
      continue;
    }

    assert(currentNode->getLiteral().isNormal());

    // 2) Rules which require context (only to normalized literals)
    // IMPORTANT: it is not sufficient to look upwards

    // we reset transitiveClosureNode to prevent too many unrelated connections
    // this is needed to remove useless literals without worrying about xrefs
    Node::transitiveClosureNode = nullptr;
    // wo do not need this because we check after each modal rule bidirectional

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
    // lazy saturation:
    // nodes that should be saturated are marked if a spurious counterexample is found
    // we do this at node level because child nodes should inherit this property
    auto saturatedLiterals = currentNode->getLiteral().saturate();
    currentNode->appendBranch(saturatedLiterals);
  }
}

// IMPORTANT: correctnes of this funtcion relies on the fact that the worklist pops positive
// literals first. (because we reuse the tableau for nomalization and do not readd any nodes to
// unreduced nodes)
bool Tableau::tryApplyModalRuleOnce(const int applyToEvent) {
  while (!unreducedNodes.isEmpty()) {
    Node *currentNode = unreducedNodes.pop();

    if (const auto result =
            Rules::applyPositiveModalRule(currentNode->getLiteral(), applyToEvent)) {
      assert(unreducedNodes.validate());
      assert(currentNode->getParentNode()->validate());

      // to detect at the world cycles
      // set it before appendBranch call
      Node::transitiveClosureNode = currentNode->getLastUnrollingParent();
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
    Node *nodeWithEquality, Node *node, const Renaming &renaming,
    std::unordered_set<Literal> &allRenamedLiterals,
    const std::unordered_map<const Node *, Node *> &originalToCopy,
    std::unordered_set<const Node *> &unrollingParents) {
  if (nodeWithEquality != node) {  // do not rename the original equality
    node->rename(renaming);
  }
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
      renameBranchesInternalDown(nodeWithEquality, *childIt, renaming, allRenamedLiteralsCopy,
                                 originalToCopy,
                                 unrollingParents);  // copy for each branching
    }
  }
  if (!isNew && !unrollingParents.contains(node)) {
    deleteNode(node);
  }
  assert(unreducedNodes.validate());
}

/*
 *  Given a node with an equality predicate, renames the branch according to the equality.
 *  The original branch is copied from the root to the node and moved from the node downwards while
 *  renaming. The renamed branch will still contain the equality predicate "e1 = e2" to remember it.
 *  All newly added nodes are automatically added to the worklist.
 *
 *  NOTE: If different literals are renamed to identical literals, only a single copy is kept.
 */
void Tableau::renameBranches(Node *node) {
  assert(validate());
  assert(node->getLiteral().operation == PredicateOperation::equality);

  // Compute renaming according to equality predicate (from larger label to lower label).
  // e1 = e2
  const int e1 = node->getLiteral().leftEvent->label.value();
  const int e2 = node->getLiteral().rightEvent->label.value();
  const int from = (e1 < e2) ? e2 : e1;
  const int to = (e1 < e2) ? e1 : e2;
  assert(from != to);
  const auto renaming = Renaming::simple(from, to);
  // TODO: node->renaming = node->renaming.totalCompose(renaming);

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
  // do not rename node, begin with children
  renameBranchesInternalDown(node, firstUnsharedNode, renaming, renamedLiterals, originalToCopy,
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

DNF simplifyDnf(const DNF &dnf) {
  // return dnf;  // To disable simplification
  Stats::diff("simplifyDnf - removed cubes").first(dnf.size());
  auto sortedDnf = dnf;
  std::ranges::sort(sortedDnf, std::less(), &Cube::size);

  DNF simplified;
  simplified.reserve(sortedDnf.size());
  for (const auto &c1 : sortedDnf) {
    const bool subsumed =
        std::ranges::any_of(simplified, [&](const Cube &c2) { return isSubsumed(c1, c2); });
    if (!subsumed) {
      simplified.push_back(c1);
    }
  }
  /*if (simplified.size() < dnf.size()) {
    std::cout << "DNF reduction: " << dnf.size() << " -> " << simplified.size() << "\n";
  }*/
  Stats::diff("simplifyDnf - removed cubes").second(simplified.size());
  return simplified;
}

size_t computeSize(const Node *node) {
  size_t size = 1;
  for (const auto &child : node->getChildren()) {
    size += computeSize(child.get());
  }
  return size;
}

DNF Tableau::computeDnf() {
  assert(validate());
  normalize();
  removeUselessLiterals();
  Stats::value("normalize size").set(computeSize(rootNode.get()));

  auto dnf = extractDNF();
  ::removeUselessLiterals(dnf);
  dnf = simplifyDnf(dnf);
  assert(validateDNF(dnf));
  assert(validate());
  assert(unreducedNodes.isEmpty());
  return dnf;
}

// ===========================================================================================
// ======================================== Printing =========================================
// ===========================================================================================

void Tableau::toDotFormat(std::ofstream &output) const {
  output << "graph {\nnode[shape=\"plaintext\"]\n";
  if (rootNode != nullptr) {
    rootNode->toDotFormat(output);
  }
  output << "}\n";
}

void Tableau::exportProof(const std::string &filename) const {
  std::ofstream file("./output/" + filename + ".dot");
  toDotFormat(file);
  file.close();
}
