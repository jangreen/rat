#include "Predicate.h"

#include <iostream>

#include "Formula.h"
#include "RegularTableau.h"
#include "parsing/LogicVisitor.h"

Predicate::Predicate(const Predicate &other)
    : operation(other.operation), identifier(other.identifier) {
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
Predicate::Predicate(const PredicateOperation operation, Set &&left, std::string identifier)
    : operation(operation), identifier(identifier) {
  leftOperand = std::make_unique<Set>(std::move(left));
}
Predicate::Predicate(const PredicateOperation operation, Set &&left, Set &&right,
                     std::string identifier)
    : operation(operation), identifier(identifier) {
  leftOperand = std::make_unique<Set>(std::move(left));
  rightOperand = std::make_unique<Set>(std::move(right));
}

Predicate::Predicate(const std::string &expression) { *this = Logic::parsePredicate(expression); }

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
  } else if ((identifier.has_value()) != (other.identifier.has_value())) {
    isEqual = false;
  } else if (identifier.has_value() && *identifier != *other.identifier) {
    isEqual = false;
  }
  return isEqual;
}

std::optional<Formula> substituteHelper(
    bool substituteRight, const std::vector<std::vector<Set::PartialPredicate>> &disjunction,
    const Set &otherOperand) {
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
        // substitute
        std::optional<Predicate> p;
        if (substituteRight) {
          p = Predicate(PredicateOperation::intersectionNonEmptiness, Set(otherOperand),
                        std::move(s));
        } else {
          p = Predicate(PredicateOperation::intersectionNonEmptiness, std::move(s),
                        Set(otherOperand));
        }
        Formula newLiteral(FormulaOperation::literal, Literal(false, std::move(*p)));
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

std::optional<Formula> Predicate::applyRule(bool negated, bool modalRules) {
  switch (operation) {
    case PredicateOperation::edge: {
      return std::nullopt;
    }
    case PredicateOperation::set: {
      return std::nullopt;
    }
    case PredicateOperation::equality: {
      return std::nullopt;
    }
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
            return Formula(FormulaOperation::bottom);
          case SetOperation::full:
            // {e}.T
            // need this since we handle negation on literal level
            return Formula(FormulaOperation::top);
          case SetOperation::singleton: {
            // {e1}.{e2}
            Predicate p(PredicateOperation::equality, Set(*leftOperand), Set(*rightOperand));
            Literal l(false, std::move(p));
            return Formula(FormulaOperation::literal, std::move(l));
          }
          case SetOperation::domain: {
            // {e}.(r.s)
            if (rightOperand->leftOperand->operation == SetOperation::singleton) {
              if (rightOperand->relation->operation == RelationOperation::base) {
                // fast path for {e1}.(a.e2) -> (e1,e2) \in a
                Predicate p(PredicateOperation::edge, Set(*leftOperand),
                            Set(*rightOperand->leftOperand), *rightOperand->relation->identifier);
                Literal l(false, std::move(p));
                return Formula(FormulaOperation::literal, std::move(l));
              }
              // 1) try reduce r.s
              auto domainResult = rightOperand->applyRule(negated, modalRules);
              if (domainResult) {
                auto disjunction = *domainResult;
                return substituteHelper(true, disjunction, *leftOperand);
              }
            } else {
              // 2) -> ({e}.r).s
              Set er(SetOperation::image, Set(*leftOperand), Relation(*rightOperand->relation));
              Predicate ers(PredicateOperation::intersectionNonEmptiness, std::move(er),
                            Set(*rightOperand->leftOperand));
              return Formula(FormulaOperation::literal, Literal(false, std::move(ers)));
            }
            return std::nullopt;
          }
          case SetOperation::image: {
            // {e}.(s.r)
            if (rightOperand->leftOperand->operation == SetOperation::singleton) {
              if (rightOperand->relation->operation == RelationOperation::base) {
                // fast path for {e1}.(e2.a) -> (e2,e1) \in a
                Predicate p(PredicateOperation::edge, Set(*rightOperand->leftOperand),
                            Set(*leftOperand), *rightOperand->relation->identifier);
                Literal l(false, std::move(p));
                return Formula(FormulaOperation::literal, std::move(l));
              }
              // 1) try reduce s.r
              auto imageResult = rightOperand->applyRule(negated, modalRules);
              if (imageResult) {
                auto disjunction = *imageResult;
                return substituteHelper(true, disjunction, *leftOperand);
              }
            } else {
              // 2) -> s.(r.{e})
              Set re(SetOperation::domain, Set(*leftOperand), Relation(*rightOperand->relation));
              Predicate sre(PredicateOperation::intersectionNonEmptiness,
                            Set(*rightOperand->leftOperand), std::move(re));
              return Formula(FormulaOperation::literal, Literal(false, std::move(sre)));
            }
            return std::nullopt;
          }
          case SetOperation::base: {
            // e.B
            Predicate p(PredicateOperation::set, Set(*leftOperand), *rightOperand->identifier);
            return Formula(FormulaOperation::literal, Literal(false, std::move(p)));
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
                          Set(*leftOperand->leftOperand), Set(*rightOperand));
            Predicate s2e(PredicateOperation::intersectionNonEmptiness,
                          Set(*leftOperand->rightOperand), Set(*rightOperand));
            Formula f1(FormulaOperation::literal, Literal(false, std::move(s1e)));
            Formula f2(FormulaOperation::literal, Literal(false, std::move(s2e)));
            Formula s1e_and_s2e(FormulaOperation::logicalAnd, std::move(f1), std::move(f2));
            return s1e_and_s2e;
          }
          case SetOperation::empty:
            // 0.{e}
            return Formula(FormulaOperation::bottom);
          case SetOperation::full:
            // T.{e}
            // need this since we handle negation on literal level
            return Formula(FormulaOperation::top);
          case SetOperation::singleton:
            // {e1}.{e2}
            // could not happen since case is already handled above
            std::cout << "[Error] This should not happen. Results may be wrong." << std::endl;
            return std::nullopt;
          case SetOperation::domain: {
            // (r.s).{e}
            if (leftOperand->leftOperand->operation == SetOperation::singleton) {
              if (leftOperand->relation->operation == RelationOperation::base) {
                // fast path for (a.e2){e1} -> (e1,e2) \in a
                Predicate p(PredicateOperation::edge, Set(*rightOperand),
                            Set(*leftOperand->leftOperand), *leftOperand->relation->identifier);
                Literal l(false, std::move(p));
                return Formula(FormulaOperation::literal, std::move(l));
              }
              // 1) try reduce r.s
              auto domainResult = leftOperand->applyRule(negated, modalRules);
              if (domainResult) {
                auto disjunction = *domainResult;
                return substituteHelper(false, disjunction, *rightOperand);
              }
            } else {
              // 2) -> ({e}.r).s
              Set er(SetOperation::image, Set(*rightOperand), Relation(*leftOperand->relation));
              Predicate ers(PredicateOperation::intersectionNonEmptiness, std::move(er),
                            Set(*leftOperand->leftOperand));
              return Formula(FormulaOperation::literal, Literal(false, std::move(ers)));
            }
            return std::nullopt;
          }
          case SetOperation::image: {
            // (s.r).{e}
            if (leftOperand->leftOperand->operation == SetOperation::singleton) {
              if (leftOperand->relation->operation == RelationOperation::base) {
                // fast path for (e2.a){e1} -> (e2,e1) \in a
                Predicate p(PredicateOperation::edge, Set(*leftOperand->leftOperand),
                            Set(*rightOperand), *leftOperand->relation->identifier);
                Literal l(false, std::move(p));
                return Formula(FormulaOperation::literal, std::move(l));
              }
              // 1) try reduce s.r
              auto imageResult = leftOperand->applyRule(negated, modalRules);
              if (imageResult) {
                auto disjunction = *imageResult;
                return substituteHelper(false, disjunction, *rightOperand);
              }
            } else {
              // 2) -> s.(r.{e})
              Set re(SetOperation::domain, Set(*rightOperand), Relation(*leftOperand->relation));
              Predicate sre(PredicateOperation::intersectionNonEmptiness,
                            Set(*leftOperand->leftOperand), std::move(re));
              return Formula(FormulaOperation::literal, Literal(false, std::move(sre)));
            }
            return std::nullopt;
          }
          case SetOperation::base: {
            // B.e
            Predicate p(PredicateOperation::set, Set(*rightOperand), *leftOperand->identifier);
            return Formula(FormulaOperation::literal, Literal(false, std::move(p)));
          }
        }
      } else {
        auto leftResult = leftOperand->applyRule(negated, modalRules);
        if (leftResult) {
          auto disjunction = *leftResult;
          return substituteHelper(false, disjunction, *rightOperand);
        }
        auto rightResult = rightOperand->applyRule(negated, modalRules);
        if (rightResult) {
          auto disjunction = *rightResult;
          return substituteHelper(true, disjunction, *leftOperand);
        }
      }
      return std::nullopt;
  }
}

bool Predicate::isNormal() const {
  switch (operation) {
    case PredicateOperation::edge:
      return true;  // TODO: maybe false!, remove all at once
    case PredicateOperation::set:
      return false;  // TODO: maybe false!
    case PredicateOperation::equality:
      return true;  // TODO: maybe false!
    case PredicateOperation::intersectionNonEmptiness:
      if (leftOperand->operation == SetOperation::singleton) {
        switch (rightOperand->operation) {
          case SetOperation::singleton:
            return false;
          case SetOperation::domain: {
            auto domainNormal = rightOperand->isNormal();
            if (domainNormal && rightOperand->leftOperand->operation == SetOperation::singleton) {
              if (rightOperand->relation->operation == RelationOperation::base) {
                // fast path
                return false;
              }
              return true;
            }
            return false;
          }
          case SetOperation::image: {
            auto imageNormal = rightOperand->isNormal();
            if (imageNormal && rightOperand->leftOperand->operation == SetOperation::singleton) {
              if (rightOperand->relation->operation == RelationOperation::base) {
                // fast path
                return false;
              }
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
            return false;
          case SetOperation::domain: {
            auto domainNormal = leftOperand->isNormal();
            if (domainNormal && leftOperand->leftOperand->operation == SetOperation::singleton) {
              if (leftOperand->relation->operation == RelationOperation::base) {
                // fast path
                return false;
              }
              return true;
            }
            return false;
          }
          case SetOperation::image: {
            auto imageNormal = leftOperand->isNormal();
            if (imageNormal && leftOperand->leftOperand->operation == SetOperation::singleton) {
              if (leftOperand->relation->operation == RelationOperation::base) {
                // fast path
                return false;
              }
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

bool Predicate::hasTopSet() const {
  switch (operation) {
    case PredicateOperation::edge:
      return false;
    case PredicateOperation::set:
      return false;
    case PredicateOperation::equality:
      return false;
    case PredicateOperation::intersectionNonEmptiness:
      return (leftOperand == nullptr || leftOperand->hasTopSet()) &&
             (rightOperand == nullptr || rightOperand->hasTopSet());
  }
}

// TODO: need generalization?
int Predicate::substitute(const Set &search, const Set &replace, int n) {
  assert(n >= 1);
  if (operation == PredicateOperation::intersectionNonEmptiness) {
    if (*leftOperand == search) {
      if (n == 1) {
        *leftOperand = replace;
        return 0;
      }
      n--;
    }
    if (*rightOperand == search) {
      if (n == 1) {
        *rightOperand = replace;
        return 0;
      }
      n--;
    }

    n = leftOperand->substitute(search, replace, n);
    if (n == 0) {
      return 0;
    }
    n = rightOperand->substitute(search, replace, n);
    return n;
  }
  return n;
}

std::vector<int> Predicate::labels() const {
  switch (operation) {
    case PredicateOperation::intersectionNonEmptiness: {
      auto leftLabels = leftOperand->labels();
      auto rightLabels = rightOperand->labels();
      leftLabels.insert(std::end(leftLabels), std::begin(rightLabels), std::end(rightLabels));
      return leftLabels;
    }
    case PredicateOperation::edge: {
      return {*leftOperand->label, *rightOperand->label};
    }
    case PredicateOperation::set: {
      return {*leftOperand->label};
    }
    case PredicateOperation::equality: {
      return {*leftOperand->label, *rightOperand->label};
    }
    default:
      return {};
  }
}

std::vector<Set> Predicate::labelBaseCombinations() const {
  switch (operation) {
    case PredicateOperation::intersectionNonEmptiness: {
      auto leftLabels = leftOperand->labelBaseCombinations();
      auto rightLabels = rightOperand->labelBaseCombinations();
      leftLabels.insert(std::end(leftLabels), std::begin(rightLabels), std::end(rightLabels));
      return leftLabels;
    }
    case PredicateOperation::edge: {
      Set leftLabel = Set(SetOperation::singleton, *leftOperand->label);
      Set rightLabel = Set(SetOperation::singleton, *rightOperand->label);
      Relation r1 = Relation(*identifier);
      Relation r2 = Relation(*identifier);

      return {Set(SetOperation::image, std::move(leftLabel), std::move(r1)),
              Set(SetOperation::domain, std::move(rightLabel), std::move(r2))};
    }
    case PredicateOperation::set: {
      return {};
    }
    case PredicateOperation::equality: {
      return {};
    }
    default:
      return {};
  }
}

void Predicate::rename(const Renaming &renaming, const bool inverse) {
  switch (operation) {
    case PredicateOperation::intersectionNonEmptiness: {
      leftOperand->rename(renaming, inverse);
      rightOperand->rename(renaming, inverse);
      return;
    }
    case PredicateOperation::edge: {
      leftOperand->rename(renaming, inverse);
      rightOperand->rename(renaming, inverse);
      return;
    }
    case PredicateOperation::set: {
      leftOperand->rename(renaming, inverse);
      return;
    }
    case PredicateOperation::equality: {
      leftOperand->rename(renaming, inverse);
      rightOperand->rename(renaming, inverse);
      return;
    }
  }
}

void Predicate::saturateBase() {
  switch (operation) {
    case PredicateOperation::edge:
      if (RegularTableau::baseAssumptions.contains(*identifier)) {
        auto assumption = RegularTableau::baseAssumptions.at(*identifier);
        Set s(SetOperation::domain, Set(*rightOperand), Relation(assumption.relation));
        Predicate p(PredicateOperation::intersectionNonEmptiness, Set(*leftOperand), std::move(s));
        swap(*this, p);
      }
      return;
    case PredicateOperation::set:
      return;
    case PredicateOperation::equality:
      return;
    case PredicateOperation::intersectionNonEmptiness:
      leftOperand->saturateBase();
      rightOperand->saturateBase();
      return;
  }
}
void Predicate::saturateId() {
  switch (operation) {
    case PredicateOperation::edge: {
      // construct master identity relation
      Relation subsetId = Relation(RelationOperation::identity);
      for (const auto assumption : RegularTableau::idAssumptions) {
        subsetId =
            Relation(RelationOperation::choice, std::move(subsetId), Relation(assumption.relation));
      }

      Relation b = Relation(*identifier);
      auto leftOperandCopy = Set(*leftOperand);
      auto leftOperand = Set(SetOperation::image, std::move(leftOperandCopy), std::move(subsetId));
      Set s(SetOperation::domain, Set(*rightOperand), std::move(b));
      Predicate p(PredicateOperation::intersectionNonEmptiness, std::move(leftOperand),
                  std::move(s));
      swap(*this, p);
      return;
    }
    case PredicateOperation::set:
      return;
    case PredicateOperation::equality:
      return;
    case PredicateOperation::intersectionNonEmptiness:
      leftOperand->saturateId();
      rightOperand->saturateId();
      return;
  }
}

std::string Predicate::toString() const {
  std::string output;
  switch (operation) {
    case PredicateOperation::edge:
      output += *identifier + "(" + leftOperand->toString() + "," + rightOperand->toString() + ")";
      break;
    case PredicateOperation::set:
      output += *identifier + "(" + leftOperand->toString() + ")";
      break;
    case PredicateOperation::equality:
      output += leftOperand->toString() + " = " + rightOperand->toString();
      break;
    case PredicateOperation::intersectionNonEmptiness:
      output += leftOperand->toString() + ";" + rightOperand->toString();
      break;
  }
  return output;
}
