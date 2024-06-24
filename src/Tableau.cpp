#include "Tableau.h"

#include <iostream>

Tableau::Tableau(const Cube &cube) {
  Node *parentNode = nullptr;
  for (const auto &literal : cube) {
    auto *newNode = new Node(parentNode, literal);

    if (parentNode == nullptr) {
      newNode->tableau = this;
      rootNode = std::unique_ptr<Node>(newNode);
    } else {
      parentNode->children.emplace_back(newNode);
    }

    unreducedNodes.push(newNode);
    parentNode = newNode;
  }
}

bool Tableau::solve(int bound) {
  while (!unreducedNodes.empty() && (bound > 0 || bound == -1)) {
    if (bound > 0) {
      bound--;
    }
    Node *currentNode = unreducedNodes.top();
    unreducedNodes.pop();

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
      const Literal &equalityLiteral = currentNode->literal;
      CanonicalSet search, replace;
      if (*equalityLiteral.leftLabel < *equalityLiteral.rightLabel) {
        // e1 = e2 , e1 < e2
        search = Set::newEvent(*equalityLiteral.rightLabel);
        replace = Set::newEvent(*equalityLiteral.leftLabel);
      } else {
        search = Set::newEvent(*equalityLiteral.leftLabel);
        replace = Set::newEvent(*equalityLiteral.rightLabel);
      }

      const Node *cur = currentNode;
      while ((cur = cur->parentNode) != nullptr) {
        // check if inside literal something can be inferred
        const auto newLiterals = substitute(cur->literal, search, replace);
        for (auto &literal : newLiterals) {
          currentNode->appendBranch(literal);
        }
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

  for (const auto &child : children) {
    DNF childDNF;
    child->dnfBuilder(childDNF);
    dnf.insert(dnf.end(), std::make_move_iterator(childDNF.begin()),
               std::make_move_iterator(childDNF.end()));
  }

  if (!literal.isNormal() || literal == TOP) {
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

std::optional<Literal> Tableau::applyRuleA() {
  while (!unreducedNodes.empty()) {
    Node *currentNode = unreducedNodes.top();
    unreducedNodes.pop();

    auto result = currentNode->applyRule(true);
    if (!result) {
      continue;
    }
    // currently remove currentNode by replacing it with dummy
    // this is needed for expandNode
    currentNode->literal = TOP;

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

bool Tableau::Node::CompareNodes::operator()(const Node *left, const Node *right) const {
  return left->literal.toString() < right->literal.toString();
  // TODO: define clever measure: should measure alpha beta rules
  // left.literal->negated < right.literal->negated;
}
