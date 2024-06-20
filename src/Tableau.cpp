#include "Tableau.h"

#include <algorithm>
#include <iostream>
#include <utility>

// FIXME: Unused constructor
Tableau::Tableau(std::initializer_list<Literal> initialLiterals)
    : Tableau(Cube(initialLiterals)) {}
Tableau::Tableau(const Cube &initialLiterals) {
  Node *currentNode = nullptr;
  for (const auto &literal : initialLiterals) {
    auto newNode = std::make_unique<Node>(this, std::move(literal));
    newNode->parentNode = currentNode;

    Node *temp = newNode.get();
    if (rootNode == nullptr) {
      rootNode = std::move(newNode);
    } else if (currentNode != nullptr) {
      newNode->parentNode->leftNode = std::move(newNode);
    }

    currentNode = temp;
    unreducedNodes.push(temp);
  }
}

bool Tableau::solve(int bound) {
  while (!unreducedNodes.empty() && (bound > 0 || bound == -1)) {
    if (bound > 0) {
      bound--;
    }
    auto currentNode = unreducedNodes.front();
    unreducedNodes.pop();
    exportProof("debug");

    // 1) Rules that just rewrite a single literal
    if (currentNode->applyRule()) {
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
      Literal equalityLiteral = currentNode->literal;
      std::optional<CanonicalSet> search, replace;
      if (*equalityLiteral.leftLabel < *equalityLiteral.rightLabel) {
        // e1 = e2 , e1 < e2
        search = Set::newEvent(*equalityLiteral.rightLabel);
        replace = Set::newEvent(*equalityLiteral.leftLabel);
      } else {
        search = Set::newEvent(*equalityLiteral.leftLabel);
        replace = Set::newEvent(*equalityLiteral.rightLabel);
      }

      Node *temp = currentNode->parentNode;
      while (temp != nullptr) {
        // check if inside literal can be something inferred
        auto newLiterals = substitute(temp->literal, *search, *replace);
        for (auto &literal : newLiterals) {
          currentNode->appendBranch(literal);
        }
        temp = temp->parentNode;
      }
    }
  }

  // warning if bound is reached
  if (bound == 0 && !unreducedNodes.empty()) {
    std::cout << "[Warning] Configured bound is reached. Answer is imprecise." << std::endl;
  }

  return rootNode->isClosed();
}

void Tableau::Node::dnfBuilder(DNF &dnf) const {
  if (isClosed()) {
    return;
  }
  if (isLeaf()) {
    if (literal.isNormal() && literal != TOP) {
      dnf.push_back({literal});
    }
    return;
  } else {
    if (leftNode != nullptr) {
      DNF leftDNF;
      leftNode->dnfBuilder(leftDNF);
      dnf.insert(dnf.end(), std::make_move_iterator(leftDNF.begin()),
                 std::make_move_iterator(leftDNF.end()));
    }
    if (rightNode != nullptr) {
      DNF rightDNF;
      rightNode->dnfBuilder(rightDNF);
      dnf.insert(dnf.end(), std::make_move_iterator(rightDNF.begin()),
                 std::make_move_iterator(rightDNF.end()));
    }
    if (literal.isNormal() && literal != TOP) {
      if (dnf.empty()) {
        dnf.emplace_back();
      }
      for (auto &cube : dnf) {
        cube.push_back(literal);
      }
    }
  }
}

DNF Tableau::Node::extractDNF() const {
  DNF dnf;
  dnfBuilder(dnf);
  return dnf;
}

std::optional<Literal> Tableau::applyRuleA() {
  while (!unreducedNodes.empty()) {
    auto currentNode = unreducedNodes.front();
    unreducedNodes.pop();

    auto result = currentNode->applyRule(true);
    if (result) {
      auto modalResult = *result;

      // currently remove currentNode by replacing it with dummy
      // this is needed for expandNode
      currentNode->literal = TOP;

      // find atomic
      for (const auto &cube : modalResult) {
        // should be only one cube
        for (const auto &literal : cube) {
          if (literal.isPositiveEdgePredicate()) {
            return literal;
          }
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

void Tableau::exportProof(const std::string& filename) const {
  std::ofstream file("./output/" + filename + ".dot");
  toDotFormat(file);
  file.close();
}
