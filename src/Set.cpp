#include "RegularTableau.h"
#include "parsing/LogicVisitor.h"

int Set::maxSingletonLabel = 0;

Set::Set(const Set &other)
    : operation(other.operation), identifier(other.identifier), label(other.label) {
  if (other.leftOperand != nullptr) {
    leftOperand = std::make_unique<Set>(*other.leftOperand);
  }
  if (other.rightOperand != nullptr) {
    rightOperand = std::make_unique<Set>(*other.rightOperand);
  }
  if (other.relation != nullptr) {
    relation = std::make_unique<Relation>(*other.relation);
  }
}
Set &Set::operator=(const Set &other) {
  Set copy(other);
  swap(*this, copy);
  return *this;
}
Set::Set(const std::string &expression) { *this = Logic::parseSet(expression); }
Set::Set(const SetOperation operation, const std::optional<std::string> &identifier)
    : operation(operation), identifier(identifier) {}
Set::Set(const SetOperation operation, int label) : operation(operation), label(label) {}
Set::Set(const SetOperation operation, Set &&left) : operation(operation) {
  leftOperand = std::make_unique<Set>(std::move(left));
}
Set::Set(const SetOperation operation, Set &&left, Set &&right) : operation(operation) {
  leftOperand = std::make_unique<Set>(std::move(left));
  rightOperand = std::make_unique<Set>(std::move(right));
}
Set::Set(const SetOperation operation, Set &&left, Relation &&relation) : operation(operation) {
  leftOperand = std::make_unique<Set>(std::move(left));
  this->relation = std::make_unique<Relation>(std::move(relation));
}

bool Set::operator==(const Set &other) const {
  if (operation != other.operation) {
    return false;
  }
  if ((leftOperand == nullptr) != (other.leftOperand == nullptr)) {
    return false;
  }
  if (leftOperand != nullptr && *leftOperand != *other.leftOperand) {
    return false;
  }
  if ((rightOperand == nullptr) != (other.rightOperand == nullptr)) {
    return false;
  }
  if (rightOperand != nullptr && *rightOperand != *other.rightOperand) {
    return false;
  }
  if ((relation == nullptr) != (other.relation == nullptr)) {
    return false;
  }
  if (relation != nullptr && *relation != *other.relation) {
    return false;
  }
  if (label.has_value() != other.label.has_value()) {
    return false;
  }
  if (label.has_value() && *label != *other.label) {
    return false;
  }
  if (identifier.has_value() != other.identifier.has_value()) {
    return false;
  }
  if (identifier.has_value() && *identifier != *other.identifier) {
    return false;
  }
  return true;
}

bool Set::operator<(const Set &other) const {
  // sort lexicographically
  return toString() < other.toString();
}

bool Set::isNormal() const {
  switch (operation) {
    case SetOperation::choice:
      return leftOperand->isNormal() && rightOperand->isNormal();
    case SetOperation::intersection:
      return leftOperand->isNormal() && rightOperand->isNormal();
    case SetOperation::singleton:
      return true;
    case SetOperation::base:
      return false;
    case SetOperation::empty:
      return true;
    case SetOperation::full:
      return true;
    case SetOperation::domain:
      if (leftOperand->operation == SetOperation::singleton) {
        return relation->operation == RelationOperation::base;
      } else {
        return leftOperand->isNormal();
      }
    case SetOperation::image:
      if (leftOperand->operation == SetOperation::singleton) {
        return relation->operation == RelationOperation::base;
      } else {
        return leftOperand->isNormal();
      }
  }
}

bool Set::hasTopSet() const {
  return (SetOperation::full == operation) ||
         (leftOperand != nullptr && leftOperand->hasTopSet()) ||
         (rightOperand != nullptr && rightOperand->hasTopSet());
}

std::vector<std::vector<Set::PartialPredicate>> substituteHelper2(
    bool substituteRight, const std::vector<std::vector<Set::PartialPredicate>> &disjunction,
    const Set &otherOperand) {
  std::vector<std::vector<Set::PartialPredicate>> resultDisjunction;
  for (const auto &conjunction : disjunction) {
    std::vector<Set::PartialPredicate> resultConjunction;
    for (const auto &predicate : conjunction) {
      if (std::holds_alternative<Literal>(predicate)) {
        Literal p = std::get<Literal>(predicate);
        resultConjunction.push_back(p);
      } else {
        Set s = std::get<Set>(predicate);
        // substitute
        if (substituteRight) {
          s = Set(SetOperation::intersection, Set(otherOperand), std::move(s));
        } else {
          s = Set(SetOperation::intersection, std::move(s), Set(otherOperand));
        }
        resultConjunction.push_back(s);
      }
    }
    resultDisjunction.push_back(resultConjunction);
  }
  return resultDisjunction;
}

std::optional<std::vector<std::vector<Set::PartialPredicate>>> Set::applyRule(bool negated,
                                                                              bool modalRules) {
  std::vector<std::vector<PartialPredicate>> result;
  switch (operation) {
    case SetOperation::singleton:
      // no rule applicable to single event constant
      return std::nullopt;
    case SetOperation::empty:
      // Rule (\bot_1):
      result = {{BOTTOM}};
      return result;
    case SetOperation::full: {
      if (negated) {
        // Rule (\neg\top_1): this case needs context (handled later)
        return std::nullopt;
      } else {
        // Rule (\top_1): [T] -> { [f] } , only if positive
        Set f(SetOperation::singleton, Set::maxSingletonLabel++);
        result = {{std::move(f)}};
        return result;
      }
    }
    case SetOperation::choice: {
      if (negated) {
        // Rule (\neg\cup_1): ~[A | B] -> { ~[A], ~[B] }
        result = {{Set(*leftOperand), Set(*rightOperand)}};
        return result;
      } else {
        // Rule (\cup_1): [A | B] -> { [A] },{ [B] }
        result = {{Set(*leftOperand)}, {Set(*rightOperand)}};
        return result;
      }
    }
    case SetOperation::intersection:
      if (leftOperand->operation == SetOperation::singleton) {
        Literal eS(negated, PredicateOperation::intersectionNonEmptiness, Set(*leftOperand),
                   Set(*rightOperand));
        Set e(*leftOperand);

        if (negated) {
          // Rule (\neg\anode\lrule):
          result = {{std::move(e)}, {std::move(eS)}};
          return result;
        } else {
          // Rule (\anode\lrule): [e & S] -> { [e], e.S }
          result = {{std::move(e), std::move(eS)}};
          return result;
        }
      } else if (rightOperand->operation == SetOperation::singleton) {
        Literal eS(negated, PredicateOperation::intersectionNonEmptiness, Set(*rightOperand),
                   Set(*leftOperand));
        Set e(*rightOperand);
        if (negated) {
          // Rule (\neg\anode\rrule):
          result = {{std::move(e)}, {std::move(eS)}};
          return result;
        } else {
          // Rule (\anode\rrule): [S & e] -> { [e], e.S }
          result = {{std::move(e), std::move(eS)}};
          return result;
        }
      } else {
        // [S1 & S2]: apply rules recursively
        auto leftResult = leftOperand->applyRule(negated, modalRules);
        if (leftResult) {
          auto disjunction = *leftResult;
          return substituteHelper2(false, disjunction, *rightOperand);
        }
        auto rightResult = rightOperand->applyRule(negated, modalRules);
        if (rightResult) {
          auto disjunction = *rightResult;
          return substituteHelper2(true, disjunction, *leftOperand);
        }
      }

      return std::nullopt;
    case SetOperation::base: {
      if (!negated) {
        // Rule (\aset): [B] -> { [f], f \in B }
        Set f(SetOperation::singleton, Set::maxSingletonLabel++);
        Literal fInB(false, PredicateOperation::set, Set(f), *identifier);
        result = {{std::move(f), std::move(fInB)}};
        return result;
      } else {
        // Rule (\neg\aset): requires context (handled later)
        return std::nullopt;
      }
    }
    case SetOperation::image:
      if (leftOperand->operation == SetOperation::singleton) {
        switch (relation->operation) {
          case RelationOperation::base: {
            // only use if want to
            if (!modalRules) {
              return std::nullopt;
            }

            if (!negated) {
              // Rule (\arel\lrule): [e.b] -> { [f], (e,f) \in b }
              Set f(SetOperation::singleton, Set::maxSingletonLabel++);
              Literal efInB(false, PredicateOperation::edge, Set(*leftOperand), Set(f),
                            *relation->identifier);
              result = {{std::move(f), std::move(efInB)}};
              return result;
            } else {
              // Rule (\neg\arel\lrule): requires context (handled later)
              return std::nullopt;
            }
          }
          case RelationOperation::cartesianProduct:
            // TODO: implement
            std::cout << "[Error] Cartesian products are currently not supported." << std::endl;
            return std::nullopt;
          case RelationOperation::choice: {
            Set er1(SetOperation::image, Set(*leftOperand), Relation(*relation->leftOperand));
            Set er2(SetOperation::image, Set(*leftOperand), Relation(*relation->rightOperand));
            if (negated) {
              // Rule (\neg\cup_2\lrule): ~[e.(r1 | r2)] -> { ~[e.r1], ~[e.r2] }
              result = {{std::move(er1), std::move(er2)}};
              return result;
            } else {
              // Rule (\cup_2\lrule): [e.(r1 | r2)] -> { [e.r1] }, { [e.r2] }
              result = {{std::move(er1)}, {std::move(er2)}};
              return result;
            }
          }
          case RelationOperation::composition: {
            // Rule (\comp_{2,2}\lrule): [e(a.b)] -> { [(e.a)b] }
            Set ea(SetOperation::image, Set(*leftOperand), Relation(*relation->leftOperand));
            Set ea_b(SetOperation::image, std::move(ea), Relation(*relation->rightOperand));
            result = {{std::move(ea_b)}};
            return result;
          }
          case RelationOperation::converse: {
            // Rule (^-1\lrule): [e.(r^-1)] -> { [r.e] }
            Set re(SetOperation::domain, Set(*leftOperand), Relation(*relation->leftOperand));
            result = {{std::move(re)}};
            return result;
          }
          case RelationOperation::empty: {
            // TODO: implement
            // Rule (\bot_2\lrule):
            std::cout << "[Error] Empty relations are currently not supported." << std::endl;
            return std::nullopt;
          }
          case RelationOperation::full: {
            // TODO: implement
            // Rule (\top_2\lrule):
            std::cout << "[Error] Full relations are currently not supported." << std::endl;
            return std::nullopt;
          }
          case RelationOperation::identity: {
            // Rule (\id\lrule): [e.id] -> { [e] }
            Set e(*leftOperand);
            result = {{std::move(e)}};
            return result;
          }
          case RelationOperation::intersection: {
            // Rule (\cap_2\lrule): [e.(r1 & r2)] -> { [e.r1 & e.r2] }
            Set er1(SetOperation::image, Set(*leftOperand), Relation(*relation->leftOperand));
            Set er2(SetOperation::image, Set(*leftOperand), Relation(*relation->rightOperand));
            Set er1_and_er2(SetOperation::intersection, std::move(er1), std::move(er2));
            result = {{std::move(er1_and_er2)}};
            return result;
          }
          case RelationOperation::transitiveClosure: {
            Set er(SetOperation::image, Set(*leftOperand), Relation(*relation->leftOperand));
            Set err_star(SetOperation::image, std::move(er), Relation(*relation));
            Set e(*leftOperand);

            if (negated) {
              // Rule (\neg*\lrule): ~[e.r*] -> { ~[(e.r)r*], ~[e] }
              result = {{std::move(err_star), std::move(e)}};
              return result;
            } else {
              // Rule (*\lrule): [e.r*] -> { [(e.r)r*] }, { [e] }
              result = {{std::move(err_star)}, {std::move(e)}};
              return result;
            }
          }
        }
      } else {
        auto leftOperandResultOptional = leftOperand->applyRule(negated, modalRules);
        if (leftOperandResultOptional) {
          auto leftOperandResult = *leftOperandResultOptional;
          for (const auto &cube : leftOperandResult) {
            std::vector<PartialPredicate> newCube;
            for (const auto &partialPredicate : cube) {
              if (std::holds_alternative<Literal>(partialPredicate)) {
                newCube.push_back(std::move(partialPredicate));
              } else {
                auto set = std::get<Set>(partialPredicate);
                // TODO: there should only be one [] inside eache {}
                // otherwise we have to instersect (&) all  []'s after before replacing
                // currently we jsut assume this is the case without further checking
                Set newSet(SetOperation::image, std::move(set), Relation(*relation));
                newCube.push_back(std::move(newSet));
              }
            }
            result.push_back(std::move(newCube));
          }
          return result;
        }
        return std::nullopt;
      }
    case SetOperation::domain:
      if (leftOperand->operation == SetOperation::singleton) {
        switch (relation->operation) {
          case RelationOperation::base: {
            // only use if want to
            if (!modalRules) {
              return std::nullopt;
            }

            if (!negated) {
              // Rule (\anode\rrule): [b.e] -> { [f], (f,e) \in b }
              Set f(SetOperation::singleton, Set::maxSingletonLabel++);
              Literal feInB(false, PredicateOperation::edge, Set(f), Set(*leftOperand),
                            *relation->identifier);
              result = {{std::move(f), std::move(feInB)}};
              return result;
            } else {
              // Rule (\neg\anode\rrule): requires context (handled later)
              return std::nullopt;
            }
          }
          case RelationOperation::cartesianProduct:
            // TODO: implement
            std::cout << "[Error] Cartesian products are currently not supported." << std::endl;
            return std::nullopt;
          case RelationOperation::choice: {
            Set r1e(SetOperation::domain, Set(*leftOperand), Relation(*relation->leftOperand));
            Set r2e(SetOperation::domain, Set(*leftOperand), Relation(*relation->rightOperand));

            if (negated) {
              // Rule (\neg\cup_2\rrule): ~[(r1 | r2).e] -> { ~[r1.e], ~[r2.e] }
              result = {{std::move(r1e), std::move(r2e)}};
              return result;
            } else {
              // Rule (\cup_2\rrule): [(r1 | r2).e] -> { [r1.e] }, { [r2.e] }
              result = {{std::move(r1e)}, {std::move(r2e)}};
              return result;
            }
          }
          case RelationOperation::composition: {
            // Rule (\comp\rrule): [(a.b)e] -> { [a(b.e)] }
            Set be(SetOperation::domain, Set(*leftOperand), Relation(*relation->rightOperand));
            Set a_be(SetOperation::domain, std::move(be), Relation(*relation->leftOperand));
            result = {{std::move(a_be)}};
            return result;
          }
          case RelationOperation::converse: {
            // Rule (^-1\rrule): [(r^-1)e] -> { [e.r] }
            Set er(SetOperation::image, Set(*leftOperand), Relation(*relation->leftOperand));
            result = {{std::move(er)}};
            return result;
          }
          case RelationOperation::empty: {
            // TODO: implement
            // Rule (\bot_2\rrule):
            std::cout << "[Error] Empty relations are currently not supported." << std::endl;
            return std::nullopt;
          }
          case RelationOperation::full: {
            // TODO: implement
            // Rule (\top_2\rrule):
            std::cout << "[Error] Full relations are currently not supported." << std::endl;
            return std::nullopt;
          }
          case RelationOperation::identity: {
            // Rule (\id\rrule): [id.e] -> { [e] }
            Set e(*leftOperand);
            result = {{std::move(e)}};
            return result;
          }
          case RelationOperation::intersection: {
            // Rule (\cap_2\rrule): [(r1 & r2).e] -> { [r1.e & r2.e] }
            Set r1e(SetOperation::domain, Set(*leftOperand), Relation(*relation->leftOperand));
            Set r2e(SetOperation::domain, Set(*leftOperand), Relation(*relation->rightOperand));
            Set r1e_and_r2e(SetOperation::intersection, std::move(r1e), std::move(r2e));
            result = {{std::move(r1e_and_r2e)}};
            return result;
          }
          case RelationOperation::transitiveClosure: {
            Set re(SetOperation::domain, Set(*leftOperand), Relation(*relation->leftOperand));
            Set r_star_re(SetOperation::domain, std::move(re), Relation(*relation));
            Set e(*leftOperand);

            if (negated) {
              // Rule (\neg*\rrule): ~[r*.e] -> { ~[r*.(r.e)], ~[e] }
              result = {{std::move(r_star_re), std::move(e)}};
              return result;
            } else {
              // Rule (*\rrule): [r*.e] -> { [r*.(r.e)] }, { [e] }
              result = {{std::move(r_star_re)}, {std::move(e)}};
              return result;
            }
          }
        }
      } else {
        auto leftOperandResultOptional = leftOperand->applyRule(negated, modalRules);
        if (leftOperandResultOptional) {
          auto leftOperandResult = *leftOperandResultOptional;
          for (const auto &cube : leftOperandResult) {
            std::vector<PartialPredicate> newCube;
            for (const auto &partialPredicate : cube) {
              if (std::holds_alternative<Literal>(partialPredicate)) {
                newCube.push_back(std::move(partialPredicate));
              } else {
                auto set = std::get<Set>(partialPredicate);
                // TODO: there should only be one [] inside eache {}
                // otherwise we have to instersect (&) all  []'s after before replacing
                // currently we jsut assume this is the case without further checking
                Set newSet(SetOperation::domain, std::move(set), Relation(*relation));
                newCube.push_back(std::move(newSet));
              }
            }
            result.push_back(std::move(newCube));
          }
          return result;
        }
        return std::nullopt;
      }
  }
}

int Set::substitute(const Set &search, const Set &replace, int n) {
  assert(n >= 1);
  if (*this == search) {
    if (n == 1) {
      *this = replace;
      return 0;
    }
    n--;
  }

  switch (operation) {
    case SetOperation::choice:
      n = leftOperand->substitute(search, replace, n);
      if (n == 0) {
        return 0;
      }
      return rightOperand->substitute(search, replace, n);
    case SetOperation::intersection:
      n = leftOperand->substitute(search, replace, n);
      if (n == 0) {
        return 0;
      }
      return rightOperand->substitute(search, replace, n);
    case SetOperation::domain:
      return leftOperand->substitute(search, replace, n);
    case SetOperation::image:
      return leftOperand->substitute(search, replace, n);
    default:
      return n;
  }
}

std::vector<int> Set::labels() const {
  switch (operation) {
    case SetOperation::choice: {
      auto leftLabels = leftOperand->labels();
      auto rightLabels = rightOperand->labels();
      leftLabels.insert(std::end(leftLabels), std::begin(rightLabels), std::end(rightLabels));
      return leftLabels;
    }
    case SetOperation::intersection: {
      auto leftLabels = leftOperand->labels();
      auto rightLabels = rightOperand->labels();
      leftLabels.insert(std::end(leftLabels), std::begin(rightLabels), std::end(rightLabels));
      return leftLabels;
    }
    case SetOperation::domain:
      return leftOperand->labels();
    case SetOperation::image:
      return leftOperand->labels();
    case SetOperation::singleton:
      return {*label};
    default:
      return {};
  }
}

std::vector<Set> Set::labelBaseCombinations() const {
  switch (operation) {
    case SetOperation::choice: {
      auto leftLabels = leftOperand->labelBaseCombinations();
      auto rightLabels = rightOperand->labelBaseCombinations();
      leftLabels.insert(std::end(leftLabels), std::begin(rightLabels), std::end(rightLabels));
      return leftLabels;
    }
    case SetOperation::intersection: {
      auto leftLabels = leftOperand->labelBaseCombinations();
      auto rightLabels = rightOperand->labelBaseCombinations();
      leftLabels.insert(std::end(leftLabels), std::begin(rightLabels), std::end(rightLabels));
      return leftLabels;
    }
    case SetOperation::domain:
      if (leftOperand->operation == SetOperation::singleton &&
          relation->operation == RelationOperation::base) {
        return {Set(*this)};
      }
      return leftOperand->labelBaseCombinations();
    case SetOperation::image:
      if (leftOperand->operation == SetOperation::singleton &&
          relation->operation == RelationOperation::base) {
        return {Set(*this)};
      }
      return leftOperand->labelBaseCombinations();
    case SetOperation::singleton:
      return {};
    default:
      return {};
  }
}

void Set::rename(const Renaming &renaming, const bool inverse) {
  if (operation == SetOperation::singleton) {
    if (inverse) {
      label = renaming[*label];
    } else {
      label = std::distance(renaming.begin(), std::find(renaming.begin(), renaming.end(), *label));
    }
  } else if (leftOperand) {
    leftOperand->rename(renaming, inverse);
    if (rightOperand) {
      rightOperand->rename(renaming, inverse);
    }
  }
}

void Set::saturateBase() {
  switch (operation) {
    case SetOperation::intersection: {
      leftOperand->saturateBase();
      rightOperand->saturateBase();
      return;
    }
    case SetOperation::domain:
    case SetOperation::image:
      if (leftOperand->operation == SetOperation::singleton) {
        // saturate base relation assumptions
        if (relation->operation == RelationOperation::base &&
            relation->saturatedBase < RegularTableau::saturationBoundBase) {
          auto relationName = *relation->identifier;
          if (RegularTableau::baseAssumptions.contains(relationName)) {
            auto assumption = RegularTableau::baseAssumptions.at(relationName);
            Relation b = Relation(RelationOperation::base, relationName);
            Relation subsetR = Relation(assumption.relation);
            Relation r = Relation(RelationOperation::choice, std::move(b), std::move(subsetR));
            Assumption::markBaseRelationsAsSaturated(r, relation->saturatedBase + 1, true);
            *relation = std::move(r);
          }
        }
      } else {
        leftOperand->saturateBase();
      }
      return;
    default:
      return;
  }
}
void Set::saturateId() {
  switch (operation) {
    case SetOperation::intersection: {
      leftOperand->saturateId();
      rightOperand->saturateId();
      return;
    }
    case SetOperation::domain:
    case SetOperation::image:
      if (leftOperand->operation == SetOperation::singleton) {
        // construct master identity relation
        Relation subsetId = Relation(RelationOperation::identity);
        for (const auto assumption : RegularTableau::idAssumptions) {
          subsetId = Relation(RelationOperation::choice, std::move(subsetId),
                              Relation(assumption.relation));
        }
        // saturate identity assumptions
        if (relation->operation == RelationOperation::base &&
            relation->saturatedId < RegularTableau::saturationBoundId) {
          // saturate identity assumptions
          auto leftOperandCopy = Set(*leftOperand);
          Relation r = std::move(subsetId);
          Assumption::markBaseRelationsAsSaturated(r, relation->saturatedId + 1, false);
          *leftOperand = Set(SetOperation::image, std::move(leftOperandCopy), std::move(r));
        }
      } else {
        leftOperand->saturateId();
      }
      return;
    default:
      return;
  }
}

std::string Set::toString() const {
  std::string output;
  switch (operation) {
    case SetOperation::singleton:
      output += "{" + std::to_string(*label) + "}";
      break;
    case SetOperation::image:
      output += "(" + leftOperand->toString() + ";" + relation->toString() + ")";
      break;
    case SetOperation::domain:
      output += "(" + relation->toString() + ";" + leftOperand->toString() + ")";
      break;
    case SetOperation::base:
      output += *identifier;
      break;
    case SetOperation::empty:
      output += "0";
      break;
    case SetOperation::full:
      output += "T";
      break;
    case SetOperation::intersection:
      output += "(" + leftOperand->toString() + " & " + rightOperand->toString() + ")";
      break;
    case SetOperation::choice:
      output += "(" + leftOperand->toString() + " | " + rightOperand->toString() + ")";
      break;
  }
  return output;
}
