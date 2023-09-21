#include "Formula.h"

#include "parsing/LogicVisitor.h"

Formula::Formula(const FormulaOperation operation) : operation(operation) {}
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
  auto isEqual = operation == other.operation;
  if ((leftOperand == nullptr) != (other.leftOperand == nullptr)) {
    isEqual = false;
  } else if (leftOperand != nullptr && *leftOperand != *other.leftOperand) {
    isEqual = false;
  } else if ((rightOperand == nullptr) != (other.rightOperand == nullptr)) {
    isEqual = false;
  } else if (rightOperand != nullptr && *rightOperand != *other.rightOperand) {
    isEqual = false;
  } else if ((literal == nullptr) != (other.literal == nullptr)) {
    isEqual = false;
  } else if (literal != nullptr && *literal != *other.literal) {
    isEqual = false;
  }
  return isEqual;
}

bool Formula::operator<(const Formula &other) const {
  // sort lexicographically
  return toString() < other.toString();
}

std::optional<std::vector<std::vector<Formula>>> Formula::applyRule(bool modalRules) {
  // std::cout << "[Solver] Apply rule to formula: " << toString() << std::endl;

  switch (operation) {
    case FormulaOperation::bottom: {
      // F -> \emptyset
      std::vector<std::vector<Formula>> result{};

      return result;
    }
    case FormulaOperation::top: {
      // T -> { }
      std::vector<std::vector<Formula>> result{{}};
      return result;
    }
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
      auto literalResult = literal->applyRule(modalRules);
      if (literalResult) {
        auto formula = *literalResult;
        std::vector<std::vector<Formula>> result{{std::move(formula)}};
        return result;
      }
      return std::nullopt;
    }
    case FormulaOperation::negation:
      switch (leftOperand->operation) {
        case FormulaOperation::bottom: {
          // ~(F) -> { }
          std::vector<std::vector<Formula>> result{{}};
          return result;
        }
        case FormulaOperation::top: {
          // ~(T) -> \emptyset
          // here we use: ~(T) -> { F }
          std::vector<std::vector<Formula>> result{{Formula(FormulaOperation::bottom)}};
          return result;
        }
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

bool Formula::isNormal() const {
  return operation == FormulaOperation::literal && literal->isNormal();
}

bool Formula::isAtomic() const {
  return operation == FormulaOperation::literal && !literal->negated &&
         literal->predicate->isAtomic();
}

// TODO: refactor:
bool Formula::isIsomorphTo(const Formula &formula) const {
  int leftLabel, rightLabel;
  std::string base;
  if (formula.literal->predicate->leftOperand->operation == SetOperation::singleton) {
    if (formula.literal->predicate->rightOperand->operation == SetOperation::domain) {
      // e1(be2)
      leftLabel = *formula.literal->predicate->leftOperand->label;
      rightLabel = *formula.literal->predicate->rightOperand->leftOperand->label;
      base = *formula.literal->predicate->rightOperand->identifier;
    } else if (formula.literal->predicate->rightOperand->operation == SetOperation::image) {
      // e1(e2b)
      rightLabel = *formula.literal->predicate->leftOperand->label;
      leftLabel = *formula.literal->predicate->rightOperand->leftOperand->label;
      base = *formula.literal->predicate->rightOperand->identifier;
    }
  } else if (formula.literal->predicate->rightOperand->operation == SetOperation::singleton) {
    if (formula.literal->predicate->leftOperand->operation == SetOperation::domain) {
      // (be2)e1
      leftLabel = *formula.literal->predicate->rightOperand->label;
      rightLabel = *formula.literal->predicate->leftOperand->leftOperand->label;
      base = *formula.literal->predicate->leftOperand->identifier;
    } else if (formula.literal->predicate->leftOperand->operation == SetOperation::image) {
      // (e2b)e1
      rightLabel = *formula.literal->predicate->rightOperand->label;
      leftLabel = *formula.literal->predicate->leftOperand->leftOperand->label;
      base = *formula.literal->predicate->leftOperand->identifier;
    }
  }
  // same for this
  int thisleftLabel, thisrightLabel;
  std::string thisbase;
  if (literal->predicate->leftOperand->operation == SetOperation::singleton) {
    if (literal->predicate->rightOperand->operation == SetOperation::domain) {
      // e1(be2)
      thisleftLabel = *literal->predicate->leftOperand->label;
      thisrightLabel = *literal->predicate->rightOperand->leftOperand->label;
      thisbase = *literal->predicate->rightOperand->identifier;
    } else if (literal->predicate->rightOperand->operation == SetOperation::image) {
      // e1(e2b)
      thisrightLabel = *literal->predicate->leftOperand->label;
      thisleftLabel = *literal->predicate->rightOperand->leftOperand->label;
      thisbase = *literal->predicate->rightOperand->identifier;
    }
  } else if (literal->predicate->rightOperand->operation == SetOperation::singleton) {
    if (literal->predicate->leftOperand->operation == SetOperation::domain) {
      // (be2)e1
      thisleftLabel = *literal->predicate->rightOperand->label;
      thisrightLabel = *literal->predicate->leftOperand->leftOperand->label;
      thisbase = *literal->predicate->leftOperand->identifier;
    } else if (literal->predicate->leftOperand->operation == SetOperation::image) {
      // (e2b)e1
      thisrightLabel = *literal->predicate->rightOperand->label;
      thisleftLabel = *literal->predicate->leftOperand->leftOperand->label;
      thisbase = *literal->predicate->leftOperand->identifier;
    }
  }

  return leftLabel == thisleftLabel && rightLabel == thisrightLabel && base == thisbase;
}

std::string Formula::toString() const {
  std::string output;
  switch (operation) {
    case FormulaOperation::bottom:
      output += "F";
      break;
    case FormulaOperation::top:
      output += "T";
      break;
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