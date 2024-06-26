#include "Tableau.h"

#include <iostream>
#include <unordered_set>

#include "utility.h"

Tableau::Tableau(const Cube &cube) {
  assert(validateCube(cube));
  // avoids the need for multiple root nodes
  Node *dummyNode = new Node(nullptr, TOP);
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
  auto parentNode = node->parentNode;

  if (node->children.empty()) {
    // insert dummy child to ensure that branch does not disappear
    Node *dummy = new Node(node, TOP);
    node->children.emplace_back(dummy);
  }

  // move all other children to parents children
  for (auto &child : node->children) {
    child->parentNode = parentNode;
  }

  parentNode->children.insert(parentNode->children.end(),
                              std::make_move_iterator(node->children.begin()),
                              std::make_move_iterator(node->children.end()));
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
    exportDebug("debug");
    assert(std::none_of(unreducedNodes.cbegin(), unreducedNodes.cend(),
                        [&](const auto unreducedNode) { return unreducedNode == currentNode; }));
    assert(unreducedNodes.validate());
    assert(currentNode->validate());
    assert(currentNode->parentNode->validate());
    assert(validate());

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
      const Literal &equalityLiteral = currentNode->literal;
      assert(equalityLiteral.rightLabel.has_value() && equalityLiteral.leftLabel.has_value());
      int from, to;
      if (*equalityLiteral.leftLabel < *equalityLiteral.rightLabel) {
        // e1 = e2 , e1 < e2
        from = *equalityLiteral.rightLabel;
        to = *equalityLiteral.leftLabel;
      } else {
        from = *equalityLiteral.leftLabel;
        to = *equalityLiteral.rightLabel;
      }

      renameBranch(currentNode, from, to);
      continue;
    }

    assert(currentNode->literal.isNormal());

    // 2) Rules which require context (only to normalized literals)
    // if you need two premises l1, l2 and comes after l1 in the branch then the rule is applied if
    // we consider l2
    // we cannot consider edge predicates first and check for premise literals upwards because the
    // conclusion could be such a predicate again
    // TODO: maybe consider lazy T evalutaion (consider T as event)

    if (currentNode->literal.hasTopSet()) {
      // Rule (~\top_1)
      assert(currentNode->literal.negated);
      currentNode->inferModalTop();
    } else if (currentNode->literal.operation == PredicateOperation::setNonEmptiness) {
      // Rule (~aL), Rule (~aR)
      currentNode->inferModal();
    } else if (currentNode->literal.isPositiveEdgePredicate()) {
      // Rule (~\top_1)
      // e=f, e\in A, (e,f)\in a, e != 0
      // Rule (~aL), Rule (~aR)
      currentNode->inferModalAtomic();
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

    auto result = currentNode->applyRule(true);
    if (!result) {
      continue;
    }
    assert(unreducedNodes.validate());
    assert(currentNode->parentNode->validate());
    removeNode(currentNode);
    // assert: apply rule has removed currentNode

    // find atomic
    for (const auto &cube : *result) {
      // should be only one cube
      for (const auto &literal : cube) {
        if (literal.isPositiveEdgePredicate()) {
          return true;
        }
      }
    }
  }

  return false;
}

void Tableau::renameBranch(Node *leaf, int from, int to) {
  assert(validate());
  assert(leaf->isLeaf());
  auto renaming = Renaming(from, to);

  // determine first node that has to be renamed
  Node *cur = leaf->parentNode;
  Node *firstToRename = nullptr;
  while (cur != nullptr) {
    if (contains(cur->literal.labels(), from)) {
      firstToRename = cur;
    }
    cur = cur->parentNode;
  }
  if (firstToRename == nullptr) {
    return;
  }

  // move until first branching (bottom-up), then copy
  bool firstBranchingNodeFound = false;
  cur = leaf->parentNode;
  std::unique_ptr<Node> copiedBranch = nullptr;
  auto lastNotRenamedNode = firstToRename->parentNode;
  std::unordered_set<Literal> allRenamedLiterals;
  while (cur != lastNotRenamedNode) {
    // do copy
    assert(cur->validate());
    Literal litCopy = Literal(cur->literal);
    litCopy.rename(renaming);

    auto [_, isNew] = allRenamedLiterals.insert(litCopy);
    if (isNew) {
      Node *renamedCur = new Node(nullptr, litCopy);
      renamedCur->tableau = cur->tableau;
      if (copiedBranch != nullptr) {
        copiedBranch->parentNode = renamedCur;
        renamedCur->children.push_back(std::move(copiedBranch));
      }
      unreducedNodes.push(renamedCur);
      copiedBranch = std::unique_ptr<Node>(renamedCur);
    }

    auto parentIsRoot = cur->parentNode == rootNode.get();
    auto parentIsBranching = cur->parentNode->children.size() > 1;
    if (!firstBranchingNodeFound && (parentIsBranching || parentIsRoot)) {
      auto curIt = std::ranges::find_if(cur->parentNode->children,
                                        [&](auto &child) { return child.get() == cur; });
      auto parentNode = cur->parentNode;  // save, because next line destroys cur
      parentNode->children.erase(curIt);
      cur = parentNode;
      firstBranchingNodeFound = true;
      continue;
    }
    cur = cur->parentNode;
  }

  assert(copiedBranch != nullptr);
  copiedBranch->parentNode = lastNotRenamedNode;
  lastNotRenamedNode->children.push_back(std::move(copiedBranch));
}

DNF Tableau::dnf() {
  solve();
  return rootNode->extractDNF();
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
