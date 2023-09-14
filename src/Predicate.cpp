#include "Predicate.h"

Predicate::Predicate(const Predicate &other) : operation(other.operation) {
  if (other.leftOperand != nullptr) {
    leftOperand = std::make_unique<Set>(*other.leftOperand);
  }
  if (other.rightOperand != nullptr) {
    rightOperand = std::make_unique<Set>(*other.rightOperand);
  }
}
Predicate &Predicate::operator=(const Predicate &other) {
  Predicate copy(other);
  swap(*this, copy);
  return *this;
}
Predicate::Predicate(const PredicateOperation operation) : operation(operation) {}
Predicate::Predicate(const PredicateOperation operation, Set &&left, Set &&right)
    : operation(operation) {
  leftOperand = std::make_unique<Set>(std::move(left));
  rightOperand = std::make_unique<Set>(std::move(right));
}

std::string Predicate::toString() const {
  std::string output;
  switch (operation) {
    case PredicateOperation::bottom:
      output += "F";
      break;
    case PredicateOperation::top:
      output += "T";
      break;
    case PredicateOperation::intersectionNonEmptiness:
      output += leftOperand->toString() + ";" + rightOperand->toString();
      break;
  }
  return output;
}
