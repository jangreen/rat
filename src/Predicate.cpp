#include "Predicate.h"

#include <iostream>

#include "Formula.h"

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

std::optional<Formula> Predicate::applyRule() {
  switch (operation) {
    case PredicateOperation::bottom:
      return std::nullopt;
    case PredicateOperation::top:
      return std::nullopt;
    case PredicateOperation::intersectionNonEmptiness:
      if (leftOperand->operation == SetOperation::singleton) {
        std::cout << "[Reasoning] Rules for {e}.S are not implemented." << std::endl;
        return std::nullopt;
      } else if (rightOperand->operation == SetOperation::singleton) {
        std::cout << "[Reasoning] Rules for S.{e} are not implemented." << std::endl;
        return std::nullopt;
      } else {
        auto leftResult = leftOperand->applyRule();
        if (leftResult) {
          auto disjunction = *leftResult;

          std::optional<Formula> disjunctionF = std::nullopt;
          for (const auto &conjunction : disjunction) {
            std::optional<Formula> conjunctionF = std::nullopt;
            for (const auto &predicate : conjunction) {
              if (std::holds_alternative<Predicate>(predicate)) {
                Predicate p = std::get<Predicate>(predicate);
                Literal l(false, std::move(p));
                Formula newConjunctionF(FormulaOperation::literal, std::move(l));

                if (!conjunctionF) {
                  conjunctionF = std::move(newConjunctionF);
                } else {
                  conjunctionF = Formula(FormulaOperation::logicalAnd, std::move(*conjunctionF),
                                         std::move(newConjunctionF));
                }
              }
            }

            if (conjunctionF) {
              if (!disjunctionF) {
                disjunctionF = std::move(conjunctionF);
              } else {
                disjunctionF = Formula(FormulaOperation::logicalOr, std::move(*disjunctionF),
                                       std::move(*conjunctionF));
              }
            }
          }
          return disjunctionF;
        }
        auto rightResult = rightOperand->applyRule();
        if (rightResult) {
          auto disjunction = *rightResult;

          std::optional<Formula> disjunctionF = std::nullopt;
          for (const auto &conjunction : disjunction) {
            std::optional<Formula> conjunctionF = std::nullopt;
            for (const auto &predicate : conjunction) {
              if (std::holds_alternative<Predicate>(predicate)) {
                Predicate p = std::get<Predicate>(predicate);
                Literal l(false, std::move(p));
                Formula newConjunctionF(FormulaOperation::literal, std::move(l));

                if (!conjunctionF) {
                  conjunctionF = std::move(newConjunctionF);
                } else {
                  conjunctionF = Formula(FormulaOperation::logicalAnd, std::move(*conjunctionF),
                                         std::move(newConjunctionF));
                }
              }
            }

            if (conjunctionF) {
              if (!disjunctionF) {
                disjunctionF = std::move(conjunctionF);
              } else {
                disjunctionF = Formula(FormulaOperation::logicalOr, std::move(*disjunctionF),
                                       std::move(*conjunctionF));
              }
            }
          }
          return disjunctionF;
        }
      }
  }
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
