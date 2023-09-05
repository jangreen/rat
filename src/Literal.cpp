#include "Literal.h"

Literal::Literal(const bool negated, Predicate &&predicate) : negated(negated) {
  predicate = std::make_unique<Predicate>(std::move(predicate));
}

std::string Literal::toString() const {
  std::string output;
  if (negated) {
    output += "~";
  }
  output += predicate->toString();
  return output;
}
