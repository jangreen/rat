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
    } else if (literal.operation != PredicateOperation::intersectionNonEmptiness) {
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
      Set search1 = Set(SetOperation::image, Set(*edgeLiteral.leftOperand),
                        Relation(RelationOperation::base, edgeLiteral.identifier));
      Set replace1 = Set(*edgeLiteral.rightOperand);
      Set search2 = Set(SetOperation::domain, Set(*edgeLiteral.rightOperand),
                        Relation(RelationOperation::base, edgeLiteral.identifier));
      Set replace2 = Set(*edgeLiteral.leftOperand);

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
    Set search = Set(SetOperation::full);
    Set replace = Set(SetOperation::singleton, label);

    auto newLiterals = substitute(literal, search, replace);
    for (auto &literal : newLiterals) {
      appendBranch(std::move(literal));
    }
  }
}
void Tableau::Node::inferModalAtomic() {
  Literal edgePredicate = literal;
  Set search1 = Set(SetOperation::image, Set(*edgePredicate.leftOperand),
                    Relation(RelationOperation::base, edgePredicate.identifier));
  Set replace1 = Set(*edgePredicate.rightOperand);
  Set search2 = Set(SetOperation::domain, Set(*edgePredicate.rightOperand),
                    Relation(RelationOperation::base, edgePredicate.identifier));
  Set replace2 = Set(*edgePredicate.leftOperand);
  // replace T -> e
  Set search12 = Set(SetOperation::full);

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

bool Tableau::Node::CompareNodes::operator()(const Node *left, const Node *right) const {
  return left->literal.toString().length() < right->literal.toString().length();
  // TODO: define clever measure: should measure alpha beta rules
  // left.literal->negated < right.literal->negated;
}
