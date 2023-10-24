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

bool Literal::operator==(const Literal &other) const {
  auto isEqual = negated == other.negated;
  if ((predicate == nullptr) != (other.predicate == nullptr)) {
    isEqual = false;
  } else if (predicate != nullptr && *predicate != *other.predicate) {
    isEqual = false;
  }
  return isEqual;
}

// negation of literals is handled here:
// the applyRule for predicates returns valid answer for positive case
std::optional<Formula> Literal::applyRule(bool modalRules) {
  switch (predicate->operation) {
    case PredicateOperation::set: {
      return std::nullopt;  // no rule applicable
    }
    case PredicateOperation::edge: {
      return std::nullopt;  // no rule applicable
    }
    case PredicateOperation::equality: {
      if (negated) {
        // ~(e1 = e2)
        if (*predicate->leftOperand->label == *predicate->rightOperand->label) {
          return Formula(FormulaOperation::bottom);
        }
      }
      return std::nullopt;  // no rule applicable in case e1 = e2
    }
    case PredicateOperation::intersectionNonEmptiness: {
      auto predicateResult = predicate->applyRule(modalRules && !negated);
      if (predicateResult) {
        auto formula = *predicateResult;
        if (negated) {
          Formula negatedFormula(FormulaOperation::negation, std::move(formula));
          return negatedFormula;
        } else {
          return formula;
        }
      }
    }
  }

  return std::nullopt;
}

bool Literal::isNormal() const {
  return predicate->operation != PredicateOperation::intersectionNonEmptiness ||
         predicate->isNormal();
}

void Literal::saturate() {
  if (negated) {
    predicate->saturate();
  }
}

std::string Literal::toString() const {
  std::string output;
  if (negated) {
    output += "~";
  }
  output += predicate->toString();
  return output;
}
