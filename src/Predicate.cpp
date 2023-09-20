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

// helper
std::optional<Formula> getFormula(
    const std::vector<std::vector<Set::PartialPredicate>> &disjunction) {
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

std::optional<Formula> Predicate::applyRule() {
  switch (operation) {
    case PredicateOperation::bottom:
      return std::nullopt;
    case PredicateOperation::top:
      return std::nullopt;
    case PredicateOperation::intersectionNonEmptiness:
      if (leftOperand->operation == SetOperation::singleton) {
        switch (rightOperand->operation) {
          case SetOperation::choice: {
            // {e}.(s1 | s2) -> {e}.s1 | {e}.s2
            Predicate es1(PredicateOperation::intersectionNonEmptiness, Set(*leftOperand),
                          Set(*rightOperand->leftOperand));
            Predicate es2(PredicateOperation::intersectionNonEmptiness, Set(*leftOperand),
                          Set(*rightOperand->rightOperand));
            Formula f1(FormulaOperation::literal, Literal(false, std::move(es1)));
            Formula f2(FormulaOperation::literal, Literal(false, std::move(es2)));
            Formula es1_or_es2(FormulaOperation::logicalOr, std::move(f1), std::move(f2));
            return es1_or_es2;
          }
          case SetOperation::intersection: {
            // {e}.(s1 & s2) -> {e}.s1 & {e}.s2
            Predicate es1(PredicateOperation::intersectionNonEmptiness, Set(*leftOperand),
                          Set(*rightOperand->leftOperand));
            Predicate es2(PredicateOperation::intersectionNonEmptiness, Set(*leftOperand),
                          Set(*rightOperand->rightOperand));
            Formula f1(FormulaOperation::literal, Literal(false, std::move(es1)));
            Formula f2(FormulaOperation::literal, Literal(false, std::move(es2)));
            Formula es1_and_es2(FormulaOperation::logicalAnd, std::move(f1), std::move(f2));
            return es1_and_es2;
          }
          case SetOperation::empty:
            // {e}.0
            std::cout << "[Reasoning] Rules for {e}.0 are not implemented." << std::endl;
            return std::nullopt;
          case SetOperation::full:
            // {e}.T
            std::cout << "[Reasoning] Rules for {e}.T are not implemented." << std::endl;
            return std::nullopt;
          case SetOperation::singleton:
            // {e1}.{e2}
            std::cout << "[Reasoning] Rules for {e1}.{e2} are not implemented." << std::endl;
            return std::nullopt;
          case SetOperation::domain: {
            // {e}.(r.s)
            // 1) try reduce r.s
            auto rightResult = rightOperand->applyRule();
            if (rightResult) {
              auto disjunction = *rightResult;
              std::optional<Formula> formulaDisjunction = std::nullopt;
              for (const auto &conjunction : disjunction) {
                std::optional<Formula> formulaConjunction = std::nullopt;
                for (const auto &predicate : conjunction) {
                  if (std::holds_alternative<Predicate>(predicate)) {
                    Predicate p = std::get<Predicate>(predicate);
                    Formula newLiteral(FormulaOperation::literal, Literal(false, std::move(p)));
                    if (!formulaConjunction) {
                      formulaConjunction = newLiteral;
                    } else {
                      formulaConjunction =
                          Formula(FormulaOperation::logicalAnd, std::move(*formulaConjunction),
                                  std::move(newLiteral));
                    }
                  } else {
                    Set s = std::get<Set>(predicate);
                    Predicate p(PredicateOperation::intersectionNonEmptiness, Set(*leftOperand),
                                std::move(s));
                    Formula newLiteral(FormulaOperation::literal, Literal(false, std::move(p)));
                    if (!formulaConjunction) {
                      formulaConjunction = newLiteral;
                    } else {
                      formulaConjunction =
                          Formula(FormulaOperation::logicalAnd, std::move(*formulaConjunction),
                                  std::move(newLiteral));
                    }
                  }

                  if (!formulaDisjunction) {
                    formulaDisjunction = formulaConjunction;
                  } else {
                    formulaDisjunction =
                        Formula(FormulaOperation::logicalOr, std::move(*formulaDisjunction),
                                std::move(*formulaConjunction));
                  }
                }
              }
              return formulaDisjunction;
            }
            // 2) -> ({e}.r).s
            Set er(SetOperation::image, Set(*leftOperand), Relation(*rightOperand->relation));
            Predicate ers(PredicateOperation::intersectionNonEmptiness, std::move(er),
                          Set(*rightOperand->leftOperand));
            return Formula(FormulaOperation::literal, Literal(false, std::move(ers)));
          }
          case SetOperation::image: {
            // {e}.(s.r)
            // 1) try reduce s.r
            // 2) -> s.(r.{e})
            std::cout << "[Reasoning] Rules for {e}.(s.r) are not implemented." << std::endl;
            return std::nullopt;
          }
        }
      } else if (rightOperand->operation == SetOperation::singleton) {
        switch (leftOperand->operation) {
          case SetOperation::choice: {
            // (s1 | s2).{e} -> s1{e}. | s2{e}.
            Predicate s1e(PredicateOperation::intersectionNonEmptiness,
                          Set(*rightOperand->leftOperand), Set(*leftOperand));
            Predicate s2e(PredicateOperation::intersectionNonEmptiness,
                          Set(*rightOperand->rightOperand), Set(*leftOperand));
            Formula f1(FormulaOperation::literal, Literal(false, std::move(s1e)));
            Formula f2(FormulaOperation::literal, Literal(false, std::move(s2e)));
            Formula s1e_or_s2e(FormulaOperation::logicalOr, std::move(f1), std::move(f2));
            return s1e_or_s2e;
          }
          case SetOperation::intersection: {
            // (s1 & s2).{e} -> s1.{e} & s2.{e}
            Predicate s1e(PredicateOperation::intersectionNonEmptiness,
                          Set(*rightOperand->leftOperand), Set(*leftOperand));
            Predicate s2e(PredicateOperation::intersectionNonEmptiness,
                          Set(*rightOperand->rightOperand), Set(*leftOperand));
            Formula f1(FormulaOperation::literal, Literal(false, std::move(s1e)));
            Formula f2(FormulaOperation::literal, Literal(false, std::move(s2e)));
            Formula s1e_and_s2e(FormulaOperation::logicalAnd, std::move(f1), std::move(f2));
            return s1e_and_s2e;
          }
          case SetOperation::empty:
            // {e}.0
            std::cout << "[Reasoning] Rules for 0.{e} are not implemented." << std::endl;
            return std::nullopt;
          case SetOperation::full:
            // {e}.T
            std::cout << "[Reasoning] Rules for T.{e} are not implemented." << std::endl;
            return std::nullopt;
          case SetOperation::singleton:
            // {e1}.{e2}
            std::cout << "[Reasoning] Rules for {e1}.{e2} are not implemented." << std::endl;
            return std::nullopt;
          case SetOperation::domain: {
            // (r.s).{e}
            // 1) try reduce r.s
            auto leftResult = leftOperand->applyRule();
            if (leftResult) {
              auto disjunction = *leftResult;
              std::optional<Formula> formulaDisjunction = std::nullopt;
              for (const auto &conjunction : disjunction) {
                std::optional<Formula> formulaConjunction = std::nullopt;
                for (const auto &predicate : conjunction) {
                  if (std::holds_alternative<Predicate>(predicate)) {
                    Predicate p = std::get<Predicate>(predicate);
                    Formula newLiteral(FormulaOperation::literal, Literal(false, std::move(p)));
                    if (!formulaConjunction) {
                      formulaConjunction = newLiteral;
                    } else {
                      formulaConjunction =
                          Formula(FormulaOperation::logicalAnd, std::move(*formulaConjunction),
                                  std::move(newLiteral));
                    }
                  } else {
                    Set s = std::get<Set>(predicate);
                    Predicate p(PredicateOperation::intersectionNonEmptiness, std::move(s),
                                Set(*rightOperand));
                    Formula newLiteral(FormulaOperation::literal, Literal(false, std::move(p)));
                    if (!formulaConjunction) {
                      formulaConjunction = newLiteral;
                    } else {
                      formulaConjunction =
                          Formula(FormulaOperation::logicalAnd, std::move(*formulaConjunction),
                                  std::move(newLiteral));
                    }
                  }

                  if (!formulaDisjunction) {
                    formulaDisjunction = formulaConjunction;
                  } else {
                    formulaDisjunction =
                        Formula(FormulaOperation::logicalOr, std::move(*formulaDisjunction),
                                std::move(*formulaConjunction));
                  }
                }
              }
              return formulaDisjunction;
            }
            // 2) -> ({e}.r).s
            Set er(SetOperation::image, Set(*rightOperand), Relation(*leftOperand->relation));
            Predicate ers(PredicateOperation::intersectionNonEmptiness, std::move(er),
                          Set(*leftOperand->leftOperand));
            return Formula(FormulaOperation::literal, Literal(false, std::move(ers)));
          }
          case SetOperation::image: {
            // (s.r).{e}
            // 1) try reduce s.r
            auto leftResult = leftOperand->applyRule();
            if (leftResult) {
              auto disjunction = *leftResult;
              std::optional<Formula> formulaDisjunction = std::nullopt;
              for (const auto &conjunction : disjunction) {
                std::optional<Formula> formulaConjunction = std::nullopt;
                for (const auto &predicate : conjunction) {
                  if (std::holds_alternative<Predicate>(predicate)) {
                    Predicate p = std::get<Predicate>(predicate);
                    Formula newLiteral(FormulaOperation::literal, Literal(false, std::move(p)));
                    if (!formulaConjunction) {
                      formulaConjunction = newLiteral;
                    } else {
                      formulaConjunction =
                          Formula(FormulaOperation::logicalAnd, std::move(*formulaConjunction),
                                  std::move(newLiteral));
                    }
                  } else {
                    Set s = std::get<Set>(predicate);
                    Predicate p(PredicateOperation::intersectionNonEmptiness, std::move(s),
                                Set(*rightOperand));
                    Formula newLiteral(FormulaOperation::literal, Literal(false, std::move(p)));
                    if (!formulaConjunction) {
                      formulaConjunction = newLiteral;
                    } else {
                      formulaConjunction =
                          Formula(FormulaOperation::logicalAnd, std::move(*formulaConjunction),
                                  std::move(newLiteral));
                    }
                  }

                  if (!formulaDisjunction) {
                    formulaDisjunction = formulaConjunction;
                  } else {
                    formulaDisjunction =
                        Formula(FormulaOperation::logicalOr, std::move(*formulaDisjunction),
                                std::move(*formulaConjunction));
                  }
                }
              }
              return formulaDisjunction;
            }
            // 2) -> s.(r.{e})
            Set re(SetOperation::domain, Set(*rightOperand), Relation(*leftOperand->relation));
            Predicate sre(PredicateOperation::intersectionNonEmptiness,
                          Set(*leftOperand->leftOperand), std::move(re));
            return Formula(FormulaOperation::literal, Literal(false, std::move(sre)));
          }
        }
      } else {
        auto leftResult = leftOperand->applyRule();
        if (leftResult) {
          auto disjunction = *leftResult;
          return getFormula(disjunction);
        }
        auto rightResult = rightOperand->applyRule();
        if (rightResult) {
          auto disjunction = *rightResult;
          return getFormula(disjunction);
        }
      }
      return std::nullopt;
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
