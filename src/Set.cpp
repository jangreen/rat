#include <cassert>
#include <unordered_set>

#include "Assumption.h"
#include "Literal.h"

int Set::maxSingletonLabel = 0;

// initialization helper
bool calcIsNormal(SetOperation operation, CanonicalSet leftOperand, CanonicalSet rightOperand,
                  CanonicalRelation relation) {
  switch (operation) {
    case SetOperation::choice:
    case SetOperation::intersection:
      assert(rightOperand != nullptr);
      return leftOperand->isNormal && rightOperand->isNormal;
    case SetOperation::singleton:
    case SetOperation::empty:
    case SetOperation::full:
      return true;
    case SetOperation::base:
      return false;
    case SetOperation::domain:
    case SetOperation::image:
      return leftOperand->operation == SetOperation::singleton
                 ? relation->operation == RelationOperation::base
                 : leftOperand->isNormal;
    default:
      assert(false);
  }
}

std::vector<int> calcLabels(SetOperation operation, CanonicalSet leftOperand,
                            CanonicalSet rightOperand, std::optional<int> label) {
  switch (operation) {
    case SetOperation::intersection:
    case SetOperation::choice: {
      auto leftLabels = leftOperand->labels;
      auto rightLabels = rightOperand->labels;
      leftLabels.insert(std::end(leftLabels), std::begin(rightLabels), std::end(rightLabels));
      return leftLabels;
    }
    case SetOperation::domain:
    case SetOperation::image:
      return leftOperand->labels;
    case SetOperation::singleton:
      return {*label};
    default:
      return {};
  }
}

std::vector<CanonicalSet> calcLabelBaseCombinations(SetOperation operation,
                                                    CanonicalSet leftOperand,
                                                    CanonicalSet rightOperand,
                                                    CanonicalRelation relation,
                                                    CanonicalSet thisRef) {
  switch (operation) {
    case SetOperation::choice:
    case SetOperation::intersection: {
      auto left = leftOperand->labelBaseCombinations;
      auto right = rightOperand->labelBaseCombinations;
      left.insert(std::end(left), std::begin(right), std::end(right));
      return left;
    }
    case SetOperation::domain:
    case SetOperation::image: {
      return leftOperand->operation == SetOperation::singleton &&
                     relation->operation == RelationOperation::base
                 ? std::vector{thisRef}
                 : leftOperand->labelBaseCombinations;
    }
    default:
      return {};
  }
}

bool calcHasTopSet(SetOperation operation, CanonicalSet leftOperand, CanonicalSet rightOperand) {
  return (SetOperation::full == operation) || (leftOperand != nullptr && leftOperand->hasTopSet) ||
         (rightOperand != nullptr && rightOperand->hasTopSet);
}

Set::Set(SetOperation operation, CanonicalSet leftOperand, CanonicalSet rightOperand,
         CanonicalRelation relation, std::optional<int> label,
         std::optional<std::string> identifier)
    : operation(operation),
      leftOperand(leftOperand),
      rightOperand(rightOperand),
      relation(relation),
      label(label),
      identifier(std::move(identifier)),
      isNormal(calcIsNormal(operation, leftOperand, rightOperand, relation)),
      labels(calcLabels(operation, leftOperand, rightOperand, label)),
      labelBaseCombinations(
          calcLabelBaseCombinations(operation, leftOperand, rightOperand, relation, this)),
      hasTopSet(calcHasTopSet(operation, leftOperand, rightOperand)) {}

Set::Set(const Set &&other) noexcept
    : operation(other.operation),
      leftOperand(other.leftOperand),
      rightOperand(other.rightOperand),
      relation(other.relation),
      identifier(other.identifier),
      isNormal(other.isNormal),
      hasTopSet(other.hasTopSet),
      labels(other.labels),
      labelBaseCombinations(other.labelBaseCombinations) {}

CanonicalSet Set::newSet(SetOperation operation, CanonicalSet left, CanonicalSet right,
                         CanonicalRelation relation, std::optional<int> label,
                         const std::optional<std::string> &identifier) {
  static std::unordered_set<Set> canonicalizer;
  auto [iter, created] = canonicalizer.emplace(operation, left, right, relation, label, identifier);
  return &(*iter);
}

CanonicalSet Set::newSet(SetOperation operation) {
  return newSet(operation, nullptr, nullptr, nullptr, std::nullopt, std::nullopt);
}
CanonicalSet Set::newSet(SetOperation operation, CanonicalSet left) {
  return newSet(operation, left, nullptr, nullptr, std::nullopt, std::nullopt);
}
CanonicalSet Set::newSet(SetOperation operation, CanonicalSet left, CanonicalSet right) {
  return newSet(operation, left, right, nullptr, std::nullopt, std::nullopt);
}

CanonicalSet Set::newSet(SetOperation operation, CanonicalSet left, CanonicalRelation relation) {
  return newSet(operation, left, nullptr, relation, std::nullopt, std::nullopt);
}
CanonicalSet Set::newEvent(int label) {
  return newSet(SetOperation::singleton, nullptr, nullptr, nullptr, label, std::nullopt);
}
CanonicalSet Set::newBaseSet(std::string &identifier) {
  return newSet(SetOperation::base, nullptr, nullptr, nullptr, std::nullopt, identifier);
}

bool Set::operator==(const Set &other) const {
  return operation == other.operation && leftOperand == other.leftOperand &&
         rightOperand == other.rightOperand && relation == other.relation && label == other.label &&
         identifier == other.identifier;
}

CanonicalSet Set::substitute(CanonicalSet search, CanonicalSet replace, int *n) const {
  if (this == search) {
    if (*n == 1) {
      return replace;
    }
    (*n)--;
    return this;
  }
  if (leftOperand != nullptr) {
    auto leftSub = leftOperand->substitute(search, replace, n);
    if (leftSub != leftOperand) {
      return Set::newSet(operation, leftSub, rightOperand, relation, label, identifier);
    }
  }
  if (rightOperand != nullptr) {
    auto rightSub = rightOperand->substitute(search, replace, n);
    if (rightSub != rightOperand) {
      return Set::newSet(operation, leftOperand, rightSub, relation, label, identifier);
    }
  }
  return this;
}

CanonicalSet Set::rename(const Renaming &renaming, bool inverse) const {
  if (operation == SetOperation::singleton) {
    if (inverse) {
      return Set::newEvent(renaming[*label]);
    } else {
      auto newLabel =
          std::distance(renaming.begin(), std::find(renaming.begin(), renaming.end(), *label));
      return Set::newEvent(int(newLabel));
    }
  }
  CanonicalSet leftRenamed;
  CanonicalSet rightRenamed;
  if (leftOperand != nullptr) {
    leftRenamed = leftOperand->rename(renaming, inverse);
  }
  if (rightOperand != nullptr) {
    rightRenamed = rightOperand->rename(renaming, inverse);
  }
  return Set::newSet(operation, leftRenamed, rightRenamed, relation, label, identifier);
}

std::vector<std::vector<PartialPredicate>> substituteHelper2(
    bool substituteRight, const std::vector<std::vector<PartialPredicate>> &disjunction,
    CanonicalSet otherOperand) {
  std::vector<std::vector<PartialPredicate>> resultDisjunction;
  resultDisjunction.reserve(disjunction.size());
  for (const auto &conjunction : disjunction) {
    std::vector<PartialPredicate> resultConjunction;
    resultConjunction.reserve(conjunction.size());
    for (const auto &predicate : conjunction) {
      if (std::holds_alternative<Literal>(predicate)) {
        auto p = std::get<Literal>(predicate);
        resultConjunction.emplace_back(p);
      } else {
        auto s = std::get<CanonicalSet>(predicate);
        // substitute
        if (substituteRight) {
          s = Set::newSet(SetOperation::intersection, otherOperand, s);
        } else {
          s = Set::newSet(SetOperation::intersection, s, otherOperand);
        }
        resultConjunction.emplace_back(s);
      }
    }
    resultDisjunction.push_back(resultConjunction);
  }
  return resultDisjunction;
}

std::optional<std::vector<std::vector<PartialPredicate>>> Set::applyRule(bool negated,
                                                                         bool modalRules) const {
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
        CanonicalSet f = Set::newEvent(Set::maxSingletonLabel++);
        result = {{f}};
        return result;
      }
    }
    case SetOperation::choice: {
      if (negated) {
        // Rule (\neg\cup_1): ~[A | B] -> { ~[A], ~[B] }
        result = {{leftOperand, rightOperand}};
        return result;
      } else {
        // Rule (\cup_1): [A | B] -> { [A] },{ [B] }
        result = {{leftOperand}, {rightOperand}};
        return result;
      }
    }
    case SetOperation::intersection:
      if (leftOperand->operation == SetOperation::singleton) {
        CanonicalSet e_and_S = Set::newSet(SetOperation::intersection, leftOperand, rightOperand);

        if (negated) {
          // Rule (~eL):
          result = {{leftOperand}, {Literal(negated, e_and_S)}};
          return result;
        } else {
          // Rule (eL): [e & S] -> { [e], e.S }
          result = {{leftOperand, Literal(negated, e_and_S)}};
          return result;
        }
      } else if (rightOperand->operation == SetOperation::singleton) {
        CanonicalSet S_and_e = Set::newSet(SetOperation::intersection, leftOperand, rightOperand);

        if (negated) {
          // Rule (~eR):
          result = {{rightOperand}, {Literal(negated, S_and_e)}};
          return result;
        } else {
          // Rule (eR): [S & e] -> { [e], S.e }
          result = {{rightOperand, Literal(negated, S_and_e)}};
          return result;
        }
      } else {
        // [S1 & S2]: apply rules recursively
        auto leftResult = leftOperand->applyRule(negated, modalRules);
        if (leftResult) {
          auto disjunction = *leftResult;
          return substituteHelper2(false, disjunction, rightOperand);
        }
        auto rightResult = rightOperand->applyRule(negated, modalRules);
        if (rightResult) {
          auto disjunction = *rightResult;
          return substituteHelper2(true, disjunction, leftOperand);
        }
      }

      return std::nullopt;
    case SetOperation::base: {
      if (!negated) {
        // Rule (A): [B] -> { [f], f \in B }
        CanonicalSet f = Set::newEvent(Set::maxSingletonLabel++);
        Literal fInB(false, *f->label, *identifier);
        result = {{f, fInB}};
        return result;
      } else {
        // Rule (~A): requires context (handled later)
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
              // Rule (aL): [e.b] -> { [f], (e,f) \in b }
              CanonicalSet f = Set::newEvent(Set::maxSingletonLabel++);
              Literal efInB(false, *leftOperand->label, *f->label, *relation->identifier);
              result = {{f, efInB}};
              return result;
            } else {
              // Rule (~aL): requires context (handled later)
              return std::nullopt;
            }
          }
          case RelationOperation::cartesianProduct:
            // TODO: implement
            std::cout << "[Error] Cartesian products are currently not supported." << std::endl;
            return std::nullopt;
          case RelationOperation::choice: {
            CanonicalSet er1 = Set::newSet(SetOperation::image, leftOperand, relation->leftOperand);
            CanonicalSet er2 =
                Set::newSet(SetOperation::image, leftOperand, relation->rightOperand);
            if (negated) {
              // Rule (~v_2L): ~[e.(r1 | r2)] -> { ~[e.r1], ~[e.r2] }
              result = {{er1, er2}};
              return result;
            } else {
              // Rule (v_2L): [e.(r1 | r2)] -> { [e.r1] }, { [e.r2] }
              result = {{er1}, {er2}};
              return result;
            }
          }
          case RelationOperation::composition: {
            // Rule (._22L): [e(a.b)] -> { [(e.a)b] }
            CanonicalSet ea = Set::newSet(SetOperation::image, leftOperand, relation->leftOperand);
            CanonicalSet ea_b = Set::newSet(SetOperation::image, ea, relation->rightOperand);
            result = {{ea_b}};
            return result;
          }
          case RelationOperation::converse: {
            // Rule (^-1L): [e.(r^-1)] -> { [r.e] }
            CanonicalSet re = Set::newSet(SetOperation::domain, leftOperand, relation->leftOperand);
            result = {{re}};
            return result;
          }
          case RelationOperation::empty: {
            // TODO: implement
            // Rule (\bot_2\lrule):
            std::cout << "[Error] Empty CanonicalRelations are currently not supported."
                      << std::endl;
            return std::nullopt;
          }
          case RelationOperation::full: {
            // TODO: implement
            // Rule (\top_2\lrule):
            std::cout << "[Error] Full CanonicalRelations are currently not supported."
                      << std::endl;
            return std::nullopt;
          }
          case RelationOperation::identity: {
            // Rule (\id\lrule): [e.id] -> { [e] }
            result = {{leftOperand}};
            return result;
          }
          case RelationOperation::intersection: {
            // Rule (\cap_2\lrule): [e.(r1 & r2)] -> { [e.r1 & e.r2] }
            CanonicalSet er1 = Set::newSet(SetOperation::image, leftOperand, relation->leftOperand);
            CanonicalSet er2 =
                Set::newSet(SetOperation::image, leftOperand, relation->rightOperand);
            CanonicalSet er1_and_er2 = Set::newSet(SetOperation::intersection, er1, er2);
            result = {{er1_and_er2}};
            return result;
          }
          case RelationOperation::transitiveClosure: {
            CanonicalSet er = Set::newSet(SetOperation::image, leftOperand, relation->leftOperand);
            CanonicalSet err_star = Set::newSet(SetOperation::image, er, relation);

            if (negated) {
              // Rule (\neg*\lrule): ~[e.r*] -> { ~[(e.r)r*], ~[e] }
              result = {{err_star, leftOperand}};
              return result;
            } else {
              // Rule (*\lrule): [e.r*] -> { [(e.r)r*] }, { [e] }
              result = {{err_star}, {leftOperand}};
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
                newCube.push_back(partialPredicate);
              } else {
                auto s = std::get<CanonicalSet>(partialPredicate);
                // TODO: there should only be one [] inside each {}
                // otherwise we have to intersect (&) all  []'s after before replacing
                // currently we just assume this is the case without further checking
                CanonicalSet newSet = Set::newSet(SetOperation::image, s, relation);
                newCube.emplace_back(newSet);
              }
            }
            result.push_back(newCube);
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
              CanonicalSet f = Set::newEvent(Set::maxSingletonLabel++);
              Literal feInB(false, *f->label, *leftOperand->label, *relation->identifier);
              result = {{f, feInB}};
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
            CanonicalSet r1e =
                Set::newSet(SetOperation::domain, leftOperand, relation->leftOperand);
            CanonicalSet r2e =
                Set::newSet(SetOperation::domain, leftOperand, relation->rightOperand);

            if (negated) {
              // Rule (\neg\cup_2\rrule): ~[(r1 | r2).e] -> { ~[r1.e], ~[r2.e] }
              result = {{r1e, r2e}};
              return result;
            } else {
              // Rule (\cup_2\rrule): [(r1 | r2).e] -> { [r1.e] }, { [r2.e] }
              result = {{r1e}, {r2e}};
              return result;
            }
          }
          case RelationOperation::composition: {
            // Rule (\comp\rrule): [(a.b)e] -> { [a(b.e)] }
            CanonicalSet be =
                Set::newSet(SetOperation::domain, leftOperand, relation->rightOperand);
            CanonicalSet a_be = Set::newSet(SetOperation::domain, be, relation->leftOperand);
            result = {{a_be}};
            return result;
          }
          case RelationOperation::converse: {
            // Rule (^-1\rrule): [(r^-1)e] -> { [e.r] }
            CanonicalSet er = Set::newSet(SetOperation::image, leftOperand, relation->leftOperand);
            result = {{er}};
            return result;
          }
          case RelationOperation::empty: {
            // TODO: implement
            // Rule (\bot_2\rrule):
            std::cout << "[Error] Empty CanonicalRelations are currently not supported."
                      << std::endl;
            return std::nullopt;
          }
          case RelationOperation::full: {
            // TODO: implement
            // Rule (\top_2\rrule):
            std::cout << "[Error] Full CanonicalRelations are currently not supported."
                      << std::endl;
            return std::nullopt;
          }
          case RelationOperation::identity: {
            // Rule (\id\rrule): [id.e] -> { [e] }
            result = {{leftOperand}};
            return result;
          }
          case RelationOperation::intersection: {
            // Rule (\cap_2\rrule): [(r1 & r2).e] -> { [r1.e & r2.e] }
            CanonicalSet r1e =
                Set::newSet(SetOperation::domain, leftOperand, relation->leftOperand);
            CanonicalSet r2e =
                Set::newSet(SetOperation::domain, leftOperand, relation->rightOperand);
            CanonicalSet r1e_and_r2e = Set::newSet(SetOperation::intersection, r1e, r2e);
            result = {{r1e_and_r2e}};
            return result;
          }
          case RelationOperation::transitiveClosure: {
            CanonicalSet re = Set::newSet(SetOperation::domain, leftOperand, relation->leftOperand);
            CanonicalSet r_star_re = Set::newSet(SetOperation::domain, re, relation);

            if (negated) {
              // Rule (\neg*\rrule): ~[r*.e] -> { ~[r*.(r.e)], ~[e] }
              result = {{r_star_re, leftOperand}};
              return result;
            } else {
              // Rule (*\rrule): [r*.e] -> { [r*.(r.e)] }, { [e] }
              result = {{r_star_re}, {leftOperand}};
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
                newCube.push_back(partialPredicate);
              } else {
                auto s = std::get<CanonicalSet>(partialPredicate);
                // TODO: there should only be one [] inside each {}
                // otherwise we have to intersect (&) all  []'s after before replacing
                // currently we just assume this is the case without further checking
                CanonicalSet newSet = Set::newSet(SetOperation::domain, s, relation);
                newCube.emplace_back(newSet);
              }
            }
            result.push_back(newCube);
          }
          return result;
        }
        return std::nullopt;
      }
  }
}

CanonicalSet Set::saturateBase() const {
  if (operation == SetOperation::domain || operation == SetOperation::image) {
    if (leftOperand->operation == SetOperation::singleton) {
      if (relation->operation == RelationOperation::base) {
        auto relationName = *relation->identifier;
        if (Assumption::baseAssumptions.contains(relationName)) {
          auto assumption = Assumption::baseAssumptions.at(relationName);
          auto r = Relation::newRelation(RelationOperation::choice, relation, assumption.relation);
          return Set::newSet(operation, leftOperand, r);
        }
        return this;
      }
    } else {
      auto leftSaturated = leftOperand->saturateBase();
      if (leftOperand != leftSaturated) {
        return Set::newSet(operation, leftSaturated, rightOperand, relation, label, identifier);
      }
      return this;
    }
  }
  CanonicalSet leftSaturated;  // FIXME: Unused (should maybe used in the return?)
  CanonicalSet rightSaturated;
  if (leftOperand != nullptr) {
    leftSaturated = leftOperand->saturateBase();
  }
  if (rightOperand != nullptr) {
    rightSaturated = rightOperand->saturateBase();
  }
  return Set::newSet(operation, saturateBase(), rightSaturated, relation, label, identifier);
}

CanonicalSet Set::saturateId() const {
  if (operation == SetOperation::domain || operation == SetOperation::image) {
    assert(leftOperand != nullptr);
    if (leftOperand->operation == SetOperation::singleton) {
      if (relation->operation == RelationOperation::base) {
        return Set::newSet(SetOperation::image, leftOperand, Assumption::masterIdRelation());
      }
    } else {
      auto leftSaturated = leftOperand->saturateId();
      if (leftOperand != leftSaturated) {
        return Set::newSet(operation, leftSaturated, rightOperand, relation, label, identifier);
      }
      return this;
    }
  }
  CanonicalSet leftSaturated;
  CanonicalSet rightSaturated;
  if (leftOperand != nullptr) {
    leftSaturated = leftOperand->saturateId();
  }
  if (rightOperand != nullptr) {
    rightSaturated = rightOperand->saturateId();
  }
  return Set::newSet(operation, leftSaturated, rightSaturated, relation, label, identifier);
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
