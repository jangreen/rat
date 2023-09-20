#include "Formula.h"

#include "parsing/LogicVisitor.h"

Formula::Formula(const Formula &other) : operation(other.operation) {
  if (other.leftOperand != nullptr) {
    leftOperand = std::make_unique<Formula>(*other.leftOperand);
  }
  if (other.rightOperand != nullptr) {
    rightOperand = std::make_unique<Formula>(*other.rightOperand);
  }
  if (other.literal != nullptr) {
    literal = std::make_unique<Literal>(*other.literal);
  }
}
Formula &Formula::operator=(const Formula &other) {
  Formula copy(other);
  swap(*this, copy);
  return *this;
}
Formula::Formula(const std::string &expression) {
  Logic visitor;
  *this = visitor.parseFormula(expression);
}
Formula::Formula(const FormulaOperation operation, Literal &&literal)
    : operation(operation), literal(std::make_unique<Literal>(std::move(literal))) {}
Formula::Formula(const FormulaOperation operation, Formula &&left)
    : operation(operation), leftOperand(std::make_unique<Formula>(std::move(left))) {}
Formula::Formula(const FormulaOperation operation, Formula &&left, Formula &&right)
    : operation(operation),
      leftOperand(std::make_unique<Formula>(std::move(left))),
      rightOperand(std::make_unique<Formula>(std::move(right))) {}

bool Formula::operator==(const Formula &other) const {
  return operation == other.operation && leftOperand == other.leftOperand &&
         rightOperand == other.rightOperand && literal == other.literal;
}

std::optional<std::vector<std::vector<Formula>>> Formula::applyRule() {
  std::cout << "[Solver] Apply rule to formula: " << toString() << std::endl;

  switch (operation) {
    case FormulaOperation::logicalAnd: {
      // f1 & f2 -> { f1, f2 }
      std::vector<std::vector<Formula>> result{{Formula(*leftOperand), Formula(*rightOperand)}};
      return result;
    }
    case FormulaOperation::logicalOr: {
      // f1 | f2 -> { f1 }, { f2 }
      std::vector<std::vector<Formula>> result{{Formula(*leftOperand)}, {Formula(*rightOperand)}};
      return result;
    }
    case FormulaOperation::literal: {
      auto literalResult = literal->applyRule();
      if (literalResult) {
        auto formula = *literalResult;
        std::vector<std::vector<Formula>> result{{std::move(formula)}};
        return result;
      }
      return std::nullopt;
    }
    case FormulaOperation::negation:
      switch (leftOperand->operation) {
        case FormulaOperation::logicalAnd: {
          // ~(f1 & f2) -> { ~f1 }, { ~f2 }
          std::vector<std::vector<Formula>> result{
              {Formula(FormulaOperation::negation, Formula(*leftOperand->leftOperand))},
              {Formula(FormulaOperation::negation, Formula(*leftOperand->rightOperand))}};
          return result;
        }
        case FormulaOperation::logicalOr: {
          // ~(f1 | f2) -> { ~f1, ~f2 }
          std::vector<std::vector<Formula>> result{
              {Formula(FormulaOperation::negation, Formula(*leftOperand->leftOperand)),
               Formula(FormulaOperation::negation, Formula(*leftOperand->rightOperand))}};
          return result;
        }
        case FormulaOperation::literal: {
          // ~(l1) -> { nl1 } (nl1 is negated literal)
          Formula f(FormulaOperation::literal,
                    Literal(true, Predicate(*leftOperand->literal->predicate)));
          std::vector<std::vector<Formula>> result{{std::move(f)}};
          return result;
        }
        case FormulaOperation::negation: {
          // ~(~f) -> { f }
          std::vector<std::vector<Formula>> result{{Formula(*leftOperand->leftOperand)}};
          return result;
        }
      }
  }
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
      output += "~(" + leftOperand->toString() + ")";
      break;
  }
  return output;
}