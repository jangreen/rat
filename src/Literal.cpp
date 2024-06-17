#include "Literal.h"

#include <iostream>

#include "RegularTableau.h"
#include "parsing/LogicVisitor.h"

Literal::Literal(const Literal &other)
    : operation(other.operation), identifier(other.identifier), negated(other.negated) {
  if (other.leftOperand != nullptr) {
    leftOperand = std::make_unique<Set>(*other.leftOperand);
  }
  if (other.rightOperand != nullptr) {
    rightOperand = std::make_unique<Set>(*other.rightOperand);
  }
}
Literal &Literal::operator=(const Literal &other) {
  Literal copy(other);
  swap(*this, copy);
  return *this;
}
Literal::Literal(const bool negated, const PredicateOperation operation, Set &&left, Set &&right)
    : operation(operation), negated(negated) {
  leftOperand = std::make_unique<Set>(std::move(left));
  rightOperand = std::make_unique<Set>(std::move(right));
}
Literal::Literal(const bool negated, const PredicateOperation operation, Set &&left,
                 std::string identifier)
    : operation(operation), identifier(identifier), negated(negated) {
  leftOperand = std::make_unique<Set>(std::move(left));
}
Literal::Literal(const bool negated, const PredicateOperation operation, Set &&left, Set &&right,
                 std::string identifier)
    : operation(operation), identifier(identifier), negated(negated) {
  leftOperand = std::make_unique<Set>(std::move(left));
  rightOperand = std::make_unique<Set>(std::move(right));
}

bool Literal::operator==(const Literal &other) const {
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
  } else if (negated != other.negated) {
    isEqual = false;
  }
  return isEqual;
}

bool Literal::operator<(const Literal &other) const {
  // sort lexicographically
  return toString() < other.toString();
}

std::optional<DNF> substituteHelper(
    bool negated, bool substituteRight,
    const std::vector<std::vector<Set::PartialPredicate>> &disjunction, const Set &otherOperand) {
  std::optional<DNF> result = std::nullopt;
  for (const auto &conjunction : disjunction) {
    std::optional<Cube> cube = std::nullopt;
    for (const auto &literal : conjunction) {
      if (std::holds_alternative<Literal>(literal)) {
        Literal l = std::get<Literal>(literal);
        if (!cube) {
          Cube c = {};
          cube = std::move(c);
        }
        cube->push_back(std::move(l));
      } else {
        Set s = std::get<Set>(literal);
        // substitute
        std::optional<Literal> l;
        if (substituteRight) {
          l = Literal(negated, PredicateOperation::intersectionNonEmptiness, Set(otherOperand),
                      std::move(s));
        } else {
          l = Literal(negated, PredicateOperation::intersectionNonEmptiness, std::move(s),
                      Set(otherOperand));
        }
        if (!cube) {
          Cube c = {};
          cube = std::move(c);
        }
        cube->push_back(std::move(*l));
      }
    }
    if (!result) {
      result = {std::move(*cube)};
    } else {
      result->push_back(std::move(*cube));
    }
  }
  return result;
}

std::optional<DNF> Literal::applyRule(bool modalRules) {
  switch (operation) {
    case PredicateOperation::edge: {
      return std::nullopt;  // no rule applicable
    }
    case PredicateOperation::set: {
      return std::nullopt;  // no rule applicable
    }
    case PredicateOperation::equality: {
      if (negated) {
        // (\neg=): ~(e1 = e2) -> F
        if (*leftOperand->label == *rightOperand->label) {
          DNF result = {{BOTTOM}};
          return result;
        }
      }
      return std::nullopt;  // no rule applicable in case e1 = e2
    }
    case PredicateOperation::intersectionNonEmptiness:
      if (leftOperand->operation == SetOperation::singleton) {
        switch (rightOperand->operation) {
          case SetOperation::choice: {
            Literal es1(negated, PredicateOperation::intersectionNonEmptiness, Set(*leftOperand),
                        Set(*rightOperand->leftOperand));
            Literal es2(negated, PredicateOperation::intersectionNonEmptiness, Set(*leftOperand),
                        Set(*rightOperand->rightOperand));

            if (negated) {
              // Rule (\neg\cup_1\lrule): ~{e}.(s1 | s2) -> ~{e}.s1, ~{e}.s2
              DNF result = {{std::move(es1), std::move(es2)}};
              return result;
            } else {
              // Rule (\cup_1\lrule): {e}.(s1 | s2) -> {e}.s1 | {e}.s2
              DNF result = {{std::move(es1)}, {std::move(es2)}};
              return result;
            }
          }
          case SetOperation::intersection: {
            Literal es1(negated, PredicateOperation::intersectionNonEmptiness, Set(*leftOperand),
                        Set(*rightOperand->leftOperand));
            Literal es2(negated, PredicateOperation::intersectionNonEmptiness, Set(*leftOperand),
                        Set(*rightOperand->rightOperand));

            if (negated) {
              // Rule (\neg\cap_1\lrule): ~{e}.(s1 & s2) -> ~{e}.s1 | ~{e}.s2
              DNF result = {{std::move(es1)}, {std::move(es2)}};
              return result;
            } else {
              // Rule (\cap_1\lrule): {e}.(s1 & s2) -> {e}.s1, {e}.s2
              DNF result = {{std::move(es1), std::move(es2)}};
              return result;
            }
          }
          case SetOperation::empty: {
            // Rule (bot_1): {e}.0
            DNF result = {{BOTTOM}};
            return result;
          }
          case SetOperation::full: {
            if (negated) {
              // Rule (\neg\top_1) + Rule (\neg=): ~{e}.T -> ~{e}.{e} -> \bot_1
              DNF result = {{BOTTOM}};
              return result;
            }
            return std::nullopt;
          }
          case SetOperation::singleton: {
            // Rule (=): {e1}.{e2}
            Literal l(negated, PredicateOperation::equality, Set(*leftOperand), Set(*rightOperand));
            DNF result = {{std::move(l)}};
            return result;
          }
          case SetOperation::domain: {
            // Rule (\comp_{1,2}): {e}.(r.s)
            if (rightOperand->leftOperand->operation == SetOperation::singleton) {
              // {e1}.(r.{e2})
              if (rightOperand->relation->operation == RelationOperation::base) {
                // fast path for {e1}.(a.e2) -> (e1,e2) \in a
                Literal l(negated, PredicateOperation::edge, Set(*leftOperand),
                          Set(*rightOperand->leftOperand), *rightOperand->relation->identifier);
                DNF result = {{std::move(l)}};
                return result;
              }
              // 1) try reduce r.s
              auto domainResult = rightOperand->applyRule(negated, modalRules);
              if (domainResult) {
                auto disjunction = *domainResult;
                return substituteHelper(negated, true, disjunction, *leftOperand);
              }
            } else {
              // 2) {e}.(r.s) -> ({e}.r).s
              Set er(SetOperation::image, Set(*leftOperand), Relation(*rightOperand->relation));
              Literal ers(negated, PredicateOperation::intersectionNonEmptiness, std::move(er),
                          Set(*rightOperand->leftOperand));
              DNF result = {{std::move(ers)}};
              return result;
            }
            return std::nullopt;
          }
          case SetOperation::image: {
            // {e}.(s.r)
            if (rightOperand->leftOperand->operation == SetOperation::singleton) {
              if (rightOperand->relation->operation == RelationOperation::base) {
                // fast path for {e1}.(e2.a) -> (e2,e1) \in a
                Literal l(negated, PredicateOperation::edge, Set(*rightOperand->leftOperand),
                          Set(*leftOperand), *rightOperand->relation->identifier);
                DNF result = {{std::move(l)}};
                return result;
              }
              // 1) try reduce s.r
              auto imageResult = rightOperand->applyRule(negated, modalRules);
              if (imageResult) {
                auto disjunction = *imageResult;
                return substituteHelper(negated, true, disjunction, *leftOperand);
              }
            } else {
              // 2) -> s.(r.{e})
              Set re(SetOperation::domain, Set(*leftOperand), Relation(*rightOperand->relation));
              Literal sre(negated, PredicateOperation::intersectionNonEmptiness,
                          Set(*rightOperand->leftOperand), std::move(re));
              DNF result = {{std::move(sre)}};
              return result;
            }
            return std::nullopt;
          }
          case SetOperation::base: {
            // e.B
            Literal l(negated, PredicateOperation::set, Set(*leftOperand),
                      *rightOperand->identifier);
            DNF result = {{std::move(l)}};
            return result;
          }
        }
      } else if (rightOperand->operation == SetOperation::singleton) {
        switch (leftOperand->operation) {
          case SetOperation::choice: {
            Literal s1e(negated, PredicateOperation::intersectionNonEmptiness,
                        Set(*rightOperand->leftOperand), Set(*leftOperand));
            Literal s2e(negated, PredicateOperation::intersectionNonEmptiness,
                        Set(*rightOperand->rightOperand), Set(*leftOperand));
            if (negated) {
              // Rule (\neg\cup_1\rrule): ~(s1 | s2).{e} -> ~s1{e}, ~s2{e}.
              DNF result = {{std::move(s1e)}, {std::move(s2e)}};
              return result;
            } else {
              // Rule (\cup_1\rrule): (s1 | s2).{e} -> s1{e}. | s2{e}.
              DNF result = {{std::move(s1e), std::move(s2e)}};
              return result;
            }
          }
          case SetOperation::intersection: {
            Literal s1e(negated, PredicateOperation::intersectionNonEmptiness,
                        Set(*leftOperand->leftOperand), Set(*rightOperand));
            Literal s2e(negated, PredicateOperation::intersectionNonEmptiness,
                        Set(*leftOperand->rightOperand), Set(*rightOperand));
            if (negated) {
              // Rule (\neg\cap_1\rrule): ~(s1 & s2).{e} -> ~s1.{e} | ~s2.{e}
              DNF result = {{std::move(s1e)}, {std::move(s2e)}};
              return result;
            } else {
              // Rule (\cap_1\rrule): (s1 & s2).{e} -> s1.{e}, s2.{e}
              DNF result = {{std::move(s1e), std::move(s2e)}};
              return result;
            }
          }
          case SetOperation::empty: {
            // Rule (bot_1): 0.{e}
            DNF result = {{BOTTOM}};
            return result;
          }
          case SetOperation::full: {
            if (negated) {
              // Rule (\neg\top_1) + Rule (\neg=): ~T.{e} -> ~{e}.{e} -> \bot_1
              DNF result = {{BOTTOM}};
              return result;
            }
            return std::nullopt;
          }
          case SetOperation::singleton: {
            // {e1}.{e2}
            // could not happen since case is already handled above
            std::cout << "[Error] This should not happen." << std::endl;
            return std::nullopt;
          }
          case SetOperation::domain: {
            // (r.s).{e}
            if (leftOperand->leftOperand->operation == SetOperation::singleton) {
              if (leftOperand->relation->operation == RelationOperation::base) {
                // fast path for (a.e2){e1} -> (e1,e2) \in a
                Literal l(negated, PredicateOperation::edge, Set(*rightOperand),
                          Set(*leftOperand->leftOperand), *leftOperand->relation->identifier);
                DNF result = {{std::move(l)}};
                return result;
              }
              // 1) try reduce r.s
              auto domainResult = leftOperand->applyRule(negated, modalRules);
              if (domainResult) {
                auto disjunction = *domainResult;
                return substituteHelper(negated, false, disjunction, *rightOperand);
              }
            } else {
              // 2) -> ({e}.r).s
              Set er(SetOperation::image, Set(*rightOperand), Relation(*leftOperand->relation));
              Literal ers(negated, PredicateOperation::intersectionNonEmptiness, std::move(er),
                          Set(*leftOperand->leftOperand));
              DNF result = {{std::move(ers)}};
              return result;
            }
            return std::nullopt;
          }
          case SetOperation::image: {
            // (s.r).{e}
            if (leftOperand->leftOperand->operation == SetOperation::singleton) {
              if (leftOperand->relation->operation == RelationOperation::base) {
                // fast path for (e2.a){e1} -> (e2,e1) \in a
                Literal l(negated, PredicateOperation::edge, Set(*leftOperand->leftOperand),
                          Set(*rightOperand), *leftOperand->relation->identifier);
                DNF result = {{std::move(l)}};
                return result;
              }
              // 1) try reduce s.r
              auto imageResult = leftOperand->applyRule(negated, modalRules);
              if (imageResult) {
                auto disjunction = *imageResult;
                return substituteHelper(negated, false, disjunction, *rightOperand);
              }
            } else {
              // 2) -> s.(r.{e})
              Set re(SetOperation::domain, Set(*rightOperand), Relation(*leftOperand->relation));
              Literal sre(negated, PredicateOperation::intersectionNonEmptiness,
                          Set(*leftOperand->leftOperand), std::move(re));
              DNF result = {{std::move(sre)}};
              return result;
            }
            return std::nullopt;
          }
          case SetOperation::base: {
            // B.e
            Literal l(negated, PredicateOperation::set, Set(*rightOperand),
                      *leftOperand->identifier);
            DNF result = {{std::move(l)}};
            return result;
          }
        }
      } else {
        auto leftResult = leftOperand->applyRule(negated, modalRules);
        if (leftResult) {
          auto disjunction = *leftResult;
          return substituteHelper(negated, false, disjunction, *rightOperand);
        }
        auto rightResult = rightOperand->applyRule(negated, modalRules);
        if (rightResult) {
          auto disjunction = *rightResult;
          return substituteHelper(negated, true, disjunction, *leftOperand);
        }
      }
      return std::nullopt;
  }
}

bool Literal::isNormal() const {
  switch (operation) {
    case PredicateOperation::edge:
      return true;  // TODO: maybe false!, remove all at once
    case PredicateOperation::set:
      return true;
    case PredicateOperation::equality:
      return true;
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

bool Literal::hasTopSet() const {
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

bool Literal::isPositiveEdgePredicate() const {
  return !negated && operation == PredicateOperation::edge;
}

bool Literal::isPositiveEqualityPredicate() const {
  return !negated && operation == PredicateOperation::equality;
}

// TODO: need generalization?
int Literal::substitute(const Set &search, const Set &replace, int n) {
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

std::vector<int> Literal::labels() const {
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

std::vector<Set> Literal::labelBaseCombinations() const {
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

void Literal::rename(const Renaming &renaming, const bool inverse) {
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

void Literal::saturateBase() {
  if (!negated) {
    return;
  }
  switch (operation) {
    case PredicateOperation::edge:
      if (RegularTableau::baseAssumptions.contains(*identifier)) {
        auto assumption = RegularTableau::baseAssumptions.at(*identifier);
        Set s(SetOperation::domain, Set(*rightOperand), Relation(assumption.relation));
        Literal l(negated, PredicateOperation::intersectionNonEmptiness, Set(*leftOperand),
                  std::move(s));
        swap(*this, l);
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
void Literal::saturateId() {
  if (!negated) {
    return;
  }
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
      Literal l(negated, PredicateOperation::intersectionNonEmptiness, std::move(leftOperand),
                std::move(s));
      swap(*this, l);
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

std::string Literal::toString() const {
  std::string output;
  if (*this == BOTTOM) {
    return "FALSE";
  }
  if (negated) {
    output += "~";
  }
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
