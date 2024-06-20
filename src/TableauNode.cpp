#include <iostream>

#include "Tableau.h"

Tableau::Node::Node(Tableau *tableau, const Literal &&literal)
    : tableau(tableau), literal(std::move(literal)) {}
Tableau::Node::Node(Node *parent, const Literal &&literal)
    : tableau(parent->tableau), literal(std::move(literal)), parentNode(parent) {}
// TODO: do this here? onlyin base case? newNode.closed = checkIfClosed(parent, relation);

bool Tableau::Node::isClosed() const {
  // TODO: lazy evaluation + save intermediate results (evaluate each node at most once)
  if (literal == BOTTOM) {
    return true;
  }
  return leftNode != nullptr && leftNode->isClosed() &&
         (rightNode == nullptr || rightNode->isClosed());
}

bool Tableau::Node::isLeaf() const { return (leftNode == nullptr) && (rightNode == nullptr); }

bool Tableau::Node::branchContains(const Literal &literal) {
  Literal negatedCopy(literal);
  negatedCopy.negated = !negatedCopy.negated;

  const Node *node = this;
  while (node != nullptr) {
    if (node->literal == literal) {
      return true;
    } else if (literal.operation != PredicateOperation::setNonEmptiness) {
      // Rule (\bot_0): check if p & ~p (only in case of atomic predicate)
      if (node->literal == negatedCopy) {
        Node newNode(this, std::move(literal));
        Node newNodeBot(&newNode, std::move(Literal(BOTTOM)));
        newNode.leftNode = std::make_unique<Node>(std::move(newNodeBot));
        leftNode = std::make_unique<Node>(std::move(newNode));
        return true;
      }
    }
    node = node->parentNode;
  }
  return false;
}

void Tableau::Node::appendBranch(const DNF &dnf) {
  if (isLeaf() && !isClosed()) {
    if (dnf.size() > 2) {
      // TODO: make this explicit using types
      std::cout << "[Bug] We would like to support only binary branching" << std::endl;
    } else if (dnf.size() > 1) {
      // only append if all resulting branches have new literals
      if (!appendable(dnf[0]) || !appendable(dnf[1])) {
        return;
      }

      // trick: lift disjunctive appendBranch to sets
      for (const auto &literal : dnf[1]) {
        appendBranch(literal);
      }
      auto temp = std::move(leftNode);
      leftNode = nullptr;
      for (const auto &literal : dnf[0]) {
        appendBranch(literal);
      }
      rightNode = std::move(temp);
    } else if (dnf.size() > 0) {
      for (const auto &literal : dnf[0]) {
        appendBranch(literal);
      }
    }
  } else {
    if (leftNode != nullptr) {
      leftNode->appendBranch(dnf);
    }
    if (rightNode != nullptr) {
      rightNode->appendBranch(dnf);
    }
  }
}

bool Tableau::Node::appendable(const Cube &cube) {
  // assume that it is called only on leafs
  // predicts conjunctive literal appending returns true if it would appended at least some literal
  if (isLeaf() && !isClosed()) {
    auto currentLeaf = this;
    for (const auto &literal : cube) {
      if (!branchContains(literal)) {
        return true;
      }
    }
  }
  return false;
}

void Tableau::Node::appendBranch(const Literal &leftLiteral) {
  if (isLeaf() && !isClosed()) {
    if (!branchContains(leftLiteral)) {
      Node newNode(this, std::move(leftLiteral));
      leftNode = std::make_unique<Node>(std::move(newNode));
      tableau->unreducedNodes.push(leftNode.get());
    }
  } else {
    if (leftNode != nullptr) {
      leftNode->appendBranch(leftLiteral);
    }
    if (rightNode != nullptr) {
      rightNode->appendBranch(leftLiteral);
    }
  }
}

void Tableau::Node::appendBranch(const Literal &leftLiteral, const Literal &rightLiteral) {
  if (isLeaf() && !isClosed()) {
    if (!branchContains(leftLiteral)) {
      Node newNode(this, std::move(leftLiteral));
      leftNode = std::make_unique<Node>(std::move(newNode));
      tableau->unreducedNodes.push(leftNode.get());
    }
    if (!branchContains(rightLiteral)) {
      Node newNode(this, std::move(rightLiteral));
      rightNode = std::make_unique<Node>(std::move(newNode));
      tableau->unreducedNodes.push(rightNode.get());
    }
  } else {
    if (leftNode != nullptr) {
      leftNode->appendBranch(leftLiteral, rightLiteral);
    }
    if (rightNode != nullptr) {
      rightNode->appendBranch(leftLiteral, rightLiteral);
    }
  }
}

std::optional<DNF> Tableau::Node::applyRule(bool modalRule) {
  auto result = literal.applyRule(modalRule);
  if (result) {
    auto disjunction = *result;
    appendBranch(disjunction);
    // make rule application in-place
    if (parentNode != nullptr && parentNode->rightNode == nullptr) {
      // important: do right first because setting parentNode->leftNode destroys 'this'
      if (rightNode != nullptr) {
        rightNode->parentNode = parentNode;
        parentNode->rightNode = std::move(rightNode);
      }
      if (leftNode != nullptr) {
        leftNode->parentNode = parentNode;
        parentNode->leftNode = std::move(leftNode);
      }
    }

    return disjunction;
  }
  return std::nullopt;
}

void Tableau::Node::inferModal() {
  if (!literal.negated) {
    return;
  }
  Node *temp = parentNode;
  while (temp != nullptr) {
    if (temp->literal.isNormal() && temp->literal.isPositiveEdgePredicate()) {
      // check if inside literal can be something inferred
      Literal edgeLiteral = temp->literal;
      // (e1, e2) \in b
      CanonicalSet e1 = Set::newEvent(*edgeLiteral.leftLabel);
      CanonicalSet e2 = Set::newEvent(*edgeLiteral.rightLabel);
      CanonicalRelation b = Relation::newBaseRelation(*edgeLiteral.identifier);
      CanonicalSet e1b = Set::newSet(SetOperation::image, e1, b);
      CanonicalSet be2 = Set::newSet(SetOperation::domain, e2, b);

      CanonicalSet search1 = e1b;
      CanonicalSet replace1 = e2;
      CanonicalSet search2 = be2;
      CanonicalSet replace2 = e1;

      auto newLiterals = substitute(literal, search1, replace1);
      for (auto &literal : newLiterals) {
        appendBranch(std::move(literal));
      }
      auto newLiterals2 = substitute(literal, search2, replace2);
      for (auto &literal : newLiterals2) {
        appendBranch(std::move(literal));
      }
    }
    temp = temp->parentNode;
  }
}

void Tableau::Node::inferModalTop() {
  if (!literal.negated) {
    return;
  }

  // get all labels
  Node *temp = parentNode;
  std::vector<int> labels;
  while (temp != nullptr) {
    if (temp->literal.isNormal() && !temp->literal.negated) {
      for (auto newLabel : temp->literal.labels()) {
        if (std::find(labels.begin(), labels.end(), newLabel) == labels.end()) {
          labels.push_back(newLabel);
        }
      }
    }
    temp = temp->parentNode;
  }

  for (auto label : labels) {
    // T -> e
    CanonicalSet search = Set::newSet(SetOperation::full);
    CanonicalSet replace = Set::newEvent(label);

    auto newLiterals = substitute(literal, search, replace);
    for (auto &literal : newLiterals) {
      appendBranch(literal);
    }
  }
}
void Tableau::Node::inferModalAtomic() {
  Literal edgeLiteral = literal;
  // (e1, e2) \in b
  CanonicalSet e1 = Set::newEvent(*edgeLiteral.leftLabel);
  CanonicalSet e2 = Set::newEvent(*edgeLiteral.rightLabel);
  CanonicalRelation b = Relation::newBaseRelation(*edgeLiteral.identifier);
  CanonicalSet e1b = Set::newSet(SetOperation::image, e1, b);
  CanonicalSet be2 = Set::newSet(SetOperation::domain, e2, b);

  CanonicalSet search1 = e1b;
  CanonicalSet replace1 = e2;
  CanonicalSet search2 = be2;
  CanonicalSet replace2 = e1;
  // replace T -> e
  CanonicalSet search12 = Set::newSet(SetOperation::full);

  Node *temp = parentNode;
  while (temp != nullptr) {
    if (temp->literal.negated && temp->literal.isNormal()) {
      // check if inside literal can be something inferred
      auto newLiterals = substitute(temp->literal, search1, replace1);
      for (auto &literal : newLiterals) {
        appendBranch(std::move(literal));
      }
      auto newLiterals2 = substitute(temp->literal, search2, replace2);
      for (auto &literal : newLiterals2) {
        appendBranch(std::move(literal));
      }
      auto newLiterals3 = substitute(temp->literal, search12, replace1);
      for (auto &literal : newLiterals3) {
        appendBranch(std::move(literal));
      }
      auto newLiterals4 = substitute(temp->literal, search12, replace2);
      for (auto &literal : newLiterals4) {
        appendBranch(std::move(literal));
      }
    }
    temp = temp->parentNode;
  }
}

void Tableau::Node::toDotFormat(std::ofstream &output) const {
  output << "N" << this << "[label=\"" << literal.toString() << "\"";
  // color closed branches
  if (isClosed()) {
    output << ", fontcolor=green";
  }
  output << "];" << std::endl;
  // children
  if (leftNode != nullptr) {
    leftNode->toDotFormat(output);
    output << "N" << this << " -- " << "N" << leftNode << ";" << std::endl;
  }
  if (rightNode != nullptr) {
    rightNode->toDotFormat(output);
    output << "N" << this << " -- " << "N" << rightNode << ";" << std::endl;
  }
}
