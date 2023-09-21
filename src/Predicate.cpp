#include "Predicate.h"

#include <iostream>

#include "Formula.h"
#include "parsing/LogicVisitor.h"

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
Predicate::Predicate(const PredicateOperation operation, Set &&left, Set &&right)
    : operation(operation) {
  leftOperand = std::make_unique<Set>(std::move(left));
  rightOperand = std::make_unique<Set>(std::move(right));
}
Predicate::Predicate(const std::string &expression) {
  Logic visitor;
  *this = visitor.parsePredicate(expression);
}

bool Predicate::operator==(const Predicate &other) const {
  auto isEqual = operation == other.operation;
  if ((leftOperand == nullptr) != (other.leftOperand == nullptr)) {
    isEqual = false;
  } else if (leftOperand != nullptr && *leftOperand != *other.leftOperand) {
    isEqual = false;
  } else if ((rightOperand == nullptr) != (other.rightOperand == nullptr)) {
    isEqual = false;
  } else if (rightOperand != nullptr && *rightOperand != *other.rightOperand) {
    isEqual = false;
  }
  return isEqual;
}

/*/ helper
std::optional<Formula> getFormula(
    const std::vector<std::vector<Set::PartialPredicate>> &disjunction) {
  std::cout << "getF: " << std::endl;
  std::optional<Formula> disjunctionF = std::nullopt;
  for (const auto &conjunction : disjunction) {
    std::cout << "|" << std::endl;
    std::optional<Formula> conjunctionF = std::nullopt;
    for (const auto &predicate : conjunction) {
      if (std::holds_alternative<Predicate>(predicate)) {
        Predicate p = std::get<Predicate>(predicate);
        std::cout << p.toString() << std::endl;
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
}*/
std::optional<Formula> substituteRight(
    const std::vector<std::vector<Set::PartialPredicate>> &disjunction, const Set &leftOperand) {
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
          formulaConjunction = Formula(FormulaOperation::logicalAnd, std::move(*formulaConjunction),
                                       std::move(newLiteral));
        }
      } else {
        Set s = std::get<Set>(predicate);
        Predicate p(PredicateOperation::intersectionNonEmptiness, Set(leftOperand), std::move(s));
        Formula newLiteral(FormulaOperation::literal, Literal(false, std::move(p)));
        if (!formulaConjunction) {
          formulaConjunction = newLiteral;
        } else {
          formulaConjunction = Formula(FormulaOperation::logicalAnd, std::move(*formulaConjunction),
                                       std::move(newLiteral));
        }
      }
    }
    if (!formulaDisjunction) {
      formulaDisjunction = formulaConjunction;
    } else {
      formulaDisjunction = Formula(FormulaOperation::logicalOr, std::move(*formulaDisjunction),
                                   std::move(*formulaConjunction));
    }
  }
  return formulaDisjunction;
}

std::optional<Formula> Predicate::applyRule(bool modalRules) {
  switch (operation) {
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
            if (*leftOperand == *rightOperand) {
              return Formula(FormulaOperation::top);
            } else {
              return Formula(FormulaOperation::bottom);
            }
          case SetOperation::domain: {
            // {e}.(r.s)
            // 1) try reduce r.s
            auto domainResult = rightOperand->applyRule(modalRules);
            if (domainResult) {
              auto disjunction = *domainResult;
              return substituteRight(disjunction, *leftOperand);
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
            auto imageResult = rightOperand->applyRule(modalRules);
            if (imageResult) {
              auto disjunction = *imageResult;
              return substituteRight(disjunction, *leftOperand);
            }
            // 2) -> s.(r.{e})
            Set re(SetOperation::domain, Set(*leftOperand), Relation(*rightOperand->relation));
            Predicate sre(PredicateOperation::intersectionNonEmptiness,
                          Set(*rightOperand->leftOperand), std::move(re));
            return Formula(FormulaOperation::literal, Literal(false, std::move(sre)));
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
            auto leftResult = leftOperand->applyRule(modalRules);
            if (leftResult) {
              auto disjunction = *leftResult;
              // TODO: use getFormula
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
            auto leftResult = leftOperand->applyRule(modalRules);
            if (leftResult) {
              auto disjunction = *leftResult;
              // TODO: use getFormula
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
        auto leftResult = leftOperand->applyRule(modalRules);
        if (leftResult) {
          auto disjunction = *leftResult;
          return substituteRight(disjunction, *rightOperand);  // TODO: maybe substituteLeft
        }
        auto rightResult = rightOperand->applyRule(modalRules);
        if (rightResult) {
          auto disjunction = *rightResult;
          return substituteRight(disjunction, *leftOperand);
        }
      }
      return std::nullopt;
  }
}

bool Predicate::isNormal() const {
  switch (operation) {
    case PredicateOperation::intersectionNonEmptiness:
      if (leftOperand->operation == SetOperation::singleton) {
        switch (rightOperand->operation) {
          case SetOperation::singleton:
            return true;
          case SetOperation::domain: {
            auto domainNormal = rightOperand->isNormal();
            if (domainNormal && rightOperand->leftOperand->operation == SetOperation::singleton) {
              return true;
            }
            return false;
          }
          case SetOperation::image: {
            auto imageNormal = rightOperand->isNormal();
            if (imageNormal && rightOperand->leftOperand->operation == SetOperation::singleton) {
              return true;
            }
            return false;
          }
          default:
            return false;
        }
      } else if (rightOperand->operation == SetOperation::singleton) {
        switch (leftOperand->operation) {
          case SetOperation::singleton:
            return true;
          case SetOperation::domain: {
            auto domainNormal = leftOperand->isNormal();
            if (domainNormal && leftOperand->leftOperand->operation == SetOperation::singleton) {
              return true;
            }
            return false;
          }
          case SetOperation::image: {
            auto imageNormal = leftOperand->isNormal();
            if (imageNormal && leftOperand->leftOperand->operation == SetOperation::singleton) {
              return true;
            }
            return false;
          }
          default:
            return false;
        }
      } else {
        return leftOperand->isNormal() && rightOperand->isNormal();
      }
  }
}

bool Predicate::substitute(const Set &search, const Set &replace) {
  if (operation == PredicateOperation::intersectionNonEmptiness) {
    if (*leftOperand == search) {
      *leftOperand = replace;
      return true;
    } else if (*rightOperand == search) {
      *rightOperand = replace;
      return true;
    } else {
      return leftOperand->substitute(search, replace) || rightOperand->substitute(search, replace);
    }
  }
  return false;
}

// example for atomic: e1(b.e2)
bool Predicate::isAtomic() const {
  if (operation != PredicateOperation::intersectionNonEmptiness) {
    return false;
  }
  bool e1_be2 = leftOperand->operation == SetOperation::singleton &&
                rightOperand->operation == SetOperation::domain &&
                rightOperand->leftOperand->operation == SetOperation::singleton &&
                rightOperand->relation->operation == RelationOperation::base;
  bool e1_e2b = leftOperand->operation == SetOperation::singleton &&
                rightOperand->operation == SetOperation::image &&
                rightOperand->leftOperand->operation == SetOperation::singleton &&
                rightOperand->relation->operation == RelationOperation::base;
  bool e1b_e2 = rightOperand->operation == SetOperation::singleton &&
                leftOperand->operation == SetOperation::image &&
                leftOperand->leftOperand->operation == SetOperation::singleton &&
                leftOperand->relation->operation == RelationOperation::base;
  bool be1_e2 = rightOperand->operation == SetOperation::singleton &&
                leftOperand->operation == SetOperation::domain &&
                leftOperand->leftOperand->operation == SetOperation::singleton &&
                leftOperand->relation->operation == RelationOperation::base;
  bool isAtomic = e1_be2 || e1_e2b || e1b_e2 || be1_e2;
  return operation == PredicateOperation::intersectionNonEmptiness && isAtomic;
}

std::vector<int> Predicate::labels() const {
  if (operation == PredicateOperation::intersectionNonEmptiness) {
    auto leftLabels = leftOperand->labels();
    auto rightLabels = rightOperand->labels();
    leftLabels.insert(std::end(leftLabels), std::begin(rightLabels), std::end(rightLabels));
    return leftLabels;
  }
  return {};
}

std::string Predicate::toString() const {
  std::string output;
  switch (operation) {
    case PredicateOperation::intersectionNonEmptiness:
      output += leftOperand->toString() + ";" + rightOperand->toString();
      break;
  }
  return output;
}
