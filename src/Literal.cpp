#include "Literal.h"

#include "Formula.h"

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

std::optional<Formula> Literal::applyRule() {
  auto predicateResult = predicate->applyRule();
  if (predicateResult) {
    auto formula = *predicateResult;
    if (negated) {
      Formula negatedFormula(FormulaOperation::negation, std::move(formula));
      return negatedFormula;
    } else {
      return formula;
    }
  }
  return std::nullopt;
}

std::string Literal::toString() const {
  std::string output;
  if (negated) {
    output += "~";
  }
  output += predicate->toString();
  return output;
}
