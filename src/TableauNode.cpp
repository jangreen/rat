#include <iostream>

#include "Tableau.h"

Tableau::Node::Node(Tableau *tableau, const Formula &&formula)
    : tableau(tableau), formula(std::move(formula)) {}
Tableau::Node::Node(Node *parent, const Formula &&formula)
    : tableau(parent->tableau), formula(std::move(formula)), parentNode(parent) {}
// TODO: do this here? onlyin base case? newNode.closed = checkIfClosed(parent, relation);

bool Tableau::Node::isClosed() const {
  // TODO: lazy evaluation + save intermediate results (evaluate each node at most once)
  if (formula.operation == FormulaOperation::bottom) {
    return true;
  }
  return leftNode && leftNode->isClosed() && (rightNode == nullptr || rightNode->isClosed());
}

bool Tableau::Node::isLeaf() const { return (leftNode == nullptr) && (rightNode == nullptr); }

bool Tableau::Node::branchContains(const Formula &formula) {
  const Node *node = this;
  while (node != nullptr) {
    if (node->formula == formula) {
      return true;
    } else if (formula.operation == FormulaOperation::literal &&
               formula.literal->predicate->operation !=
                   PredicateOperation::intersectionNonEmptiness) {
      Formula negatedCopy(formula);
      negatedCopy.literal->negated = !negatedCopy.literal->negated;
      // check if p & ~p (only in case of atomic predicate)
      if (node->formula == negatedCopy) {
        Node newNode(this, std::move(formula));
        leftNode = std::make_unique<Node>(std::move(newNode));
        Node newNode2(leftNode.get(), Formula(FormulaOperation::bottom));
        leftNode->leftNode = std::make_unique<Node>(std::move(newNode2));
        return true;
      }
    }
    node = node->parentNode;
  }
  return false;
}

void Tableau::Node::appendBranch(const GDNF &formulas) {
  if (isLeaf() && !isClosed()) {
    if (formulas.size() > 2) {
      // TODO: make this explicit using types
      std::cout << "[Bug] We would like to support only binary branching" << std::endl;
    } else if (formulas.size() > 1) {
      // special case: appending X | \emptyset to B would result in B.X and B.nullptr
      // but B.nullptr should mean here B.emptyset however this is not reflected
      // Solution: we do not append anything since truth of the empty branch implies truth of B.X
      if (!appendable(formulas[0]) || !appendable(formulas[1])) {
        return;
      }

      // trick: lift disjunctive appendBranch to sets
      for (const auto &formula : formulas[1]) {
        appendBranch(formula);
      }
      auto temp = std::move(leftNode);
      leftNode = nullptr;
      for (const auto &formula : formulas[0]) {
        appendBranch(formula);
      }
      rightNode = std::move(temp);
    } else if (formulas.size() > 0) {
      for (const auto &formula : formulas[0]) {
        appendBranch(formula);
      }
    }
  } else {
    if (leftNode != nullptr) {
      leftNode->appendBranch(formulas);
    }
    if (rightNode != nullptr) {
      rightNode->appendBranch(formulas);
    }
  }
}

bool Tableau::Node::appendable(const FormulaSet &formulas) {
  // assume that it is called only on leafs
  // predicts conjunctive formula appending returns true if it would appended at least some literal
  if (isLeaf() && !isClosed()) {
    auto currentLeaf = this;
    for (const auto &formula : formulas) {
      if (!branchContains(formula)) {
        return true;
      }
    }
  }
  return false;
}

void Tableau::Node::appendBranch(const Formula &leftFormula) {
  if (isLeaf() && !isClosed()) {
    if (!branchContains(leftFormula)) {
      Node newNode(this, std::move(leftFormula));
      leftNode = std::make_unique<Node>(std::move(newNode));
      tableau->unreducedNodes.push(leftNode.get());
      // std::cout << "[Tableau]: Append Node. " << leftNode->formula.toString() << std::endl;
    }
  } else {
    if (leftNode != nullptr) {
      leftNode->appendBranch(leftFormula);
    }
    if (rightNode != nullptr) {
      rightNode->appendBranch(leftFormula);
    }
  }
}

void Tableau::Node::appendBranch(const Formula &leftFormula, const Formula &rightFormula) {
  if (isLeaf() && !isClosed()) {
    if (!branchContains(leftFormula)) {
      Node newNode(this, std::move(leftFormula));
      leftNode = std::make_unique<Node>(std::move(newNode));
      tableau->unreducedNodes.push(leftNode.get());
    }
    if (!branchContains(rightFormula)) {
      Node newNode(this, std::move(rightFormula));
      rightNode = std::make_unique<Node>(std::move(newNode));
      tableau->unreducedNodes.push(rightNode.get());
    }
  } else {
    if (leftNode != nullptr) {
      leftNode->appendBranch(leftFormula, rightFormula);
    }
    if (rightNode != nullptr) {
      rightNode->appendBranch(leftFormula, rightFormula);
    }
  }
}

std::optional<GDNF> Tableau::Node::applyRule(bool modalRule) {
  auto result = formula.applyRule(modalRule);
  if (result) {
    auto disjunction = *result;
    appendBranch(disjunction);
    return disjunction;
  }
  return std::nullopt;
}

void Tableau::Node::inferModal() {
  if (formula.operation != FormulaOperation::literal || !formula.literal->negated) {
    return;
  }
  Node *temp = parentNode;
  while (temp != nullptr) {
    if (temp->formula.isNormal() && temp->formula.isEdgePredicate()) {
      // check if inside formula can be something inferred
      Predicate copy = *formula.literal->predicate;
      Predicate edgePredicate = *temp->formula.literal->predicate;
      Set search1 = Set(SetOperation::image, Set(*edgePredicate.leftOperand),
                        Relation(RelationOperation::base, edgePredicate.identifier));
      Set replace1 = Set(*edgePredicate.rightOperand);
      Set search2 = Set(SetOperation::domain, Set(*edgePredicate.rightOperand),
                        Relation(RelationOperation::base, edgePredicate.identifier));
      Set replace2 = Set(*edgePredicate.leftOperand);
      if (copy.substitute(search1, replace1)) {
        appendBranch(Formula(FormulaOperation::literal, Literal(true, std::move(copy))));
      } else if (copy.substitute(search2, replace2)) {
        appendBranch(Formula(FormulaOperation::literal, Literal(true, std::move(copy))));
      }
    }
    temp = temp->parentNode;
  }
}

void Tableau::Node::inferModalAtomic() {
  Predicate edgePredicate = *formula.literal->predicate;
  Set search1 = Set(SetOperation::image, Set(*edgePredicate.leftOperand),
                    Relation(RelationOperation::base, edgePredicate.identifier));
  Set replace1 = Set(*edgePredicate.rightOperand);
  Set search2 = Set(SetOperation::domain, Set(*edgePredicate.rightOperand),
                    Relation(RelationOperation::base, edgePredicate.identifier));
  Set replace2 = Set(*edgePredicate.leftOperand);

  Node *temp = parentNode;
  while (temp != nullptr) {
    if (temp->formula.operation == FormulaOperation::literal && temp->formula.literal->negated &&
        temp->formula.isNormal()) {
      // check if inside formula can be something inferred
      Predicate copy = *temp->formula.literal->predicate;
      if (copy.substitute(search1, replace1)) {
        appendBranch(Formula(FormulaOperation::literal, Literal(true, std::move(copy))));
      } else if (copy.substitute(search2, replace2)) {
        appendBranch(Formula(FormulaOperation::literal, Literal(true, std::move(copy))));
      }
    }
    temp = temp->parentNode;
  }
}

void Tableau::Node::toDotFormat(std::ofstream &output) const {
  output << "N" << this << "[label=\"" << formula.toString() << "\"";
  // color closed branches
  if (isClosed()) {
    output << ", fontcolor=green";
  }
  output << "];" << std::endl;
  // children
  if (leftNode != nullptr) {
    leftNode->toDotFormat(output);
    output << "N" << this << " -- "
           << "N" << leftNode << ";" << std::endl;
  }
  if (rightNode != nullptr) {
    rightNode->toDotFormat(output);
    output << "N" << this << " -- "
           << "N" << rightNode << ";" << std::endl;
  }
}

bool Tableau::Node::CompareNodes::operator()(const Node *left, const Node *right) const {
  return left->formula.toString().length() < right->formula.toString().length();
  // TODO: define clever measure: should measure alpha beta rules
  // left.formula->negated < right.formula->negated;
}
