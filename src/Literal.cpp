#include "Literal.h"

Literal::Literal(const Literal &other) : negated(other.negated) {
  if (other.predicate != nullptr) {
    predicate = std::make_unique<Predicate>(*other.predicate);
  }
}
Literal &Literal::operator=(const Literal &other) {
  Literal copy(other);
  swap(*this, copy);
  return *this;
}
Literal::Literal(const bool negated, Predicate &&predicate) : negated(negated) {
  this->predicate = std::make_unique<Predicate>(std::move(predicate));
}

std::string Literal::toString() const {
  std::string output;
  if (negated) {
    output += "~";
  }
  output += predicate->toString();
  return output;
}
