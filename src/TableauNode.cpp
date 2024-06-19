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
  if (children.empty()) {
    return false;
  }
  for (const auto &child : children) {
    if (!child->isClosed()) {
      return false;
    }
  }
  return true;
}

bool Tableau::Node::isLeaf() const { return children.empty(); }

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
        auto newNode = std::make_unique<Node>(this, std::move(literal));
        auto newNodeBot = std::make_unique<Node>(newNode.get(), std::move(Literal(BOTTOM)));
        newNode->children.push_back(std::move(newNodeBot));
        children.push_back(std::move(newNode));
        return true;
      }
    }
    node = node->parentNode;
  }
  return false;
}

void Tableau::Node::appendBranch(const DNF &dnf) {
  if (isLeaf() && !isClosed()) {
    for (const auto &cube : dnf) {
      // only append if all resulting branches have new literals
      if (!appendable(cube)) {
        return;
      }
    }
    for (const auto &cube : dnf) {
      auto firstLiteral = cube.front();
      auto newNode = std::make_unique<Node>(this, std::move(firstLiteral));
      for (auto &literal : cube) {
        newNode->appendBranch(literal);
      }
      tableau->unreducedNodes.push(newNode.get());
      children.push_back(std::move(newNode));
    }
  } else {
    for (const auto &child : children) {
      child->appendBranch(dnf);
    }
  }
}

bool Tableau::Node::appendable(const Cube &cube) {
  // procondition: called on leaf node
  if (isLeaf() && !isClosed()) {
    for (const auto &literal : cube) {
      if (!branchContains(literal)) {
        return true;
      }
    }
  }
  return false;
}

void Tableau::Node::appendBranch(const Literal &literal) {
  if (isLeaf() && !isClosed()) {
    if (!branchContains(literal)) {
      auto newNode = std::make_unique<Node>(this, std::move(literal));
      tableau->unreducedNodes.push(newNode.get());
      children.push_back(std::move(newNode));
    }
  } else {
    for (const auto &child : children) {
      child->appendBranch(literal);
    }
  }
}

void Tableau::Node::appendBranch(const Literal &leftLiteral, const Literal &rightLiteral) {
  if (isLeaf() && !isClosed()) {
    if (!branchContains(leftLiteral)) {
      auto newNode = std::make_unique<Node>(this, std::move(leftLiteral));
      tableau->unreducedNodes.push(newNode.get());
      children.push_back(std::move(newNode));
    }
    if (!branchContains(rightLiteral)) {
      auto newNode = std::make_unique<Node>(this, std::move(rightLiteral));
      tableau->unreducedNodes.push(newNode.get());
      children.push_back(std::move(newNode));
    }
  } else {
    for (const auto &child : children) {
      child->appendBranch(leftLiteral, rightLiteral);
    }
  }
}

std::optional<DNF> Tableau::Node::applyRule(bool modalRule) {
  auto result = literal.applyRule(modalRule);
  if (result) {
    auto disjunction = *result;
    appendBranch(disjunction);
    // make rule application in-place
    if (parentNode != nullptr) {
      for (auto &child : children) {
        child->parentNode = parentNode;
      }

      parentNode->children.insert(parentNode->children.end(),
                                  std::make_move_iterator(children.begin()),
                                  std::make_move_iterator(children.end()));
      // next line destroys this
      parentNode->children.erase(
          std::remove_if(std::begin(parentNode->children), std::end(parentNode->children),
                         [this](auto &element) { return element.get() == this; }));
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
  output << "N" << this << "[tooltip=\"";
  // debug
  output << this << std::endl << std::endl;
  // label/cube
  output << "\",label=\"" << literal.toString() << "\"";
  // color closed branches
  if (isClosed()) {
    output << ", fontcolor=green";
  }
  output << "];" << std::endl;
  // children
  for (const auto &child : children) {
    child->toDotFormat(output);
    output << "N" << this << " -- " << "N" << child << ";" << std::endl;
  }
}
