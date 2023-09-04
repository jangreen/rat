#include "Formula.h"

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