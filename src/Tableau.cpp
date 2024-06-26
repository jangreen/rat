#include "Tableau.h"

#include <iostream>

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

void Tableau::removeNode(Node *node) {
  // remove Node from unreducedNodes
  unreducedNodes.erase(node);
  assert(unreducedNodes.validate());
  assert(std::none_of(unreducedNodes.cbegin(), unreducedNodes.cend(),
                      [&](const auto unreducedNode) { return unreducedNode == node; }));
  assert(node != rootNode.get());
  assert(node->parentNode != nullptr);  // there is always a dummy root node
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
}

bool Tableau::solve(int bound) {
  while (!unreducedNodes.isEmpty() && (bound > 0 || bound == -1)) {
    if (bound > 0) {
      bound--;
    }

    Node *currentNode = unreducedNodes.pop();
    assert(std::none_of(unreducedNodes.cbegin(), unreducedNodes.cend(),
                        [&](const auto unreducedNode) { return unreducedNode == currentNode; }));
    assert(unreducedNodes.validate());
    assert(currentNode->validate());

    // 1) Rules that just rewrite a single literal
    if (currentNode->applyRule()) {
      assert(unreducedNodes.validate());
      removeNode(currentNode);  // in-place rule application
      continue;
    }

    // 2) Rules which require context (only to normalized literals)
    if (!currentNode->literal.isNormal()) {
      continue;
    }

    if (currentNode->literal.hasTopSet()) {
      // Rule (~\top_1)
      currentNode->inferModalTop();
    } else if (currentNode->literal.operation == PredicateOperation::setNonEmptiness) {
      // Rule (~a)
      currentNode->inferModal();
    } else if (currentNode->literal.isPositiveEdgePredicate()) {
      // Rule (~a), Rule (~\top_1)
      currentNode->inferModalAtomic();
    } else if (currentNode->literal.isPositiveEqualityPredicate()) {
      // Rule (\equiv)
      const Literal &equalityLiteral = currentNode->literal;
      assert(equalityLiteral.rightLabel.has_value() && equalityLiteral.leftLabel.has_value());
      CanonicalSet search, replace;
      if (*equalityLiteral.leftLabel < *equalityLiteral.rightLabel) {
        // e1 = e2 , e1 < e2
        search = Set::newEvent(*equalityLiteral.rightLabel);
        replace = Set::newEvent(*equalityLiteral.leftLabel);
      } else {
        search = Set::newEvent(*equalityLiteral.leftLabel);
        replace = Set::newEvent(*equalityLiteral.rightLabel);
      }

      Node *cur = currentNode;
      while ((cur = cur->parentNode) != nullptr) {
        // check if inside literal something can be inferred
        Literal copy = Literal(cur->literal);
        if (copy.substitute(search, replace, -1)) {
          currentNode->appendBranch(copy);
          removeNode(cur);
        }
      }
    }
  }

  // warning if bound is reached
  if (bound == 0 && !unreducedNodes.isEmpty()) {
    std::cout << "[Warning] Configured bound is reached. Answer is imprecise." << std::endl;
  }

  return rootNode->isClosed();
}

std::optional<Literal> Tableau::applyRuleA() {
  while (!unreducedNodes.isEmpty()) {
    Node *currentNode = unreducedNodes.pop();

    auto result = currentNode->applyRule(true);
    if (!result) {
      continue;
    }
    assert(unreducedNodes.validate());
    removeNode(currentNode);
    // assert: apply rule has removed currentNode

    // find atomic
    for (const auto &cube : *result) {
      // should be only one cube
      for (const auto &literal : cube) {
        if (literal.isPositiveEdgePredicate()) {
          return literal;
        }
      }
    }
  }

  return std::nullopt;
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
