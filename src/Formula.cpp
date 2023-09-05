#include "Formula.h"

// RULES
template <>
std::optional<std::tuple<Formula, Formula>> Formula::applyRule<ProofRule::land>() {
  if (operation == FormulaOperation::logicalAnd) {
    // f1 & f2 -> f1 and f2
    std::tuple<Formula, Formula> result{std::move(*leftOperand), std::move(*rightOperand)};
    return result;
  }
  return std::nullopt;
}

template <>
std::optional<std::tuple<Formula, Formula>> Formula::applyRule<ProofRule::lor>() {
  if (operation == FormulaOperation::logicalOr) {
    // f1 | f2 -> f1 or f2
    std::tuple<Formula, Formula> result{std::move(*leftOperand), std::move(*rightOperand)};
    return result;
  }
  return std::nullopt;
}

std::string Formula::toString() const {
  std::string output;
  switch (operation) {
    case FormulaOperation::literal:
      output += literal->toString();
      break;
    case FormulaOperation::logicalAnd:
      output += "(" + leftOperand->toString() + " & " + rightOperand->toString() + ")";
      break;
    case FormulaOperation::logicalOr:
      output += "(" + leftOperand->toString() + " | " + rightOperand->toString() + ")";
      break;
    case FormulaOperation::negation:
      output += "~" + leftOperand->toString();
      break;
  }
  return output;
}