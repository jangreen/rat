#include <spdlog/spdlog.h>

#include <cassert>
#include <unordered_set>

#include "Assumption.h"
#include "Literal.h"

namespace {
// ---------------------- Anonymous helper functions ----------------------

std::optional<PartialDNF> applyRelationalRule(const Literal *context, CanonicalSet event,
                                              CanonicalRelation relation, SetOperation operation,
                                              bool modalRules) {
  switch (relation->operation) {
    case RelationOperation::base: {
      // only use if want to
      if (!modalRules) {
        return std::nullopt;
      }

      if (!context->negated) {
        // RuleDirection::Left: [e.b] -> { [f], (e,f) \in b }
        // -> Rule (aL)
        // RuleDirection::Right: [b.e] -> { [f], (f,e) \in b }
        // -> Rule (aR)
        CanonicalSet f = Set::newEvent(Set::maxSingletonLabel++);
        auto b = *relation->identifier;
        int first = operation == SetOperation::image ? *event->label : *f->label;
        int second = operation == SetOperation::image ? *f->label : *event->label;

        return PartialDNF{{f, Literal(false, first, second, b)}};
      } else {
        // Rule (~aL), Rule (~aR): requires context (handled later)
        return std::nullopt;
      }
    }
    case RelationOperation::cartesianProduct:
      // TODO: implement
      spdlog::error("[Error] Cartesian products are currently not supported.");
      assert(false);
    case RelationOperation::choice: {
      // RuleDirection::Left:
      //    Rule (~v_2L): ~[e.(r1 | r2)] -> { ~[e.r1], ~[e.r2] }
      //    Rule (v_2L): [e.(r1 | r2)] -> { [e.r1] }, { [e.r2] }
      // RuleDirection::Right: [b.e] -> { [f], (f,e) \in b }
      //    Rule (~v_2R): ~[(r1 | r2).e] -> { ~[r1.e], ~[r2.e] }
      //    Rule (v_2R):
      CanonicalSet er1 = Set::newSet(operation, event, relation->leftOperand);
      CanonicalSet er2 = Set::newSet(operation, event, relation->rightOperand);
      return context->negated ? PartialDNF{{er1, er2}} : PartialDNF{{er1}, {er2}};
    }
    case RelationOperation::composition: {
      // Rule (._22L): [e(a.b)] -> { [(e.a)b] }
      CanonicalSet ea = Set::newSet(operation, event, relation->leftOperand);
      CanonicalSet ea_b = Set::newSet(operation, ea, relation->rightOperand);
      return PartialDNF{{ea_b}};
    }
    case RelationOperation::converse: {
      // Rule (^-1L): [e.(r^-1)] -> { [r.e] }
      SetOperation oppositOp =
          operation == SetOperation::image ? SetOperation::domain : SetOperation::image;
      CanonicalSet re = Set::newSet(oppositOp, event, relation->leftOperand);
      return PartialDNF{{re}};
    }
    case RelationOperation::empty: {
      // TODO: implement
      // Rule (\bot_2\lrule):
      spdlog::error("[Error] Empty relations are currently not supported.");
      assert(false);
    }
    case RelationOperation::full: {
      // TODO: implement
      // Rule (\top_2\lrule):
      spdlog::error("[Error] Full relations are currently not supported.");
      assert(false);
    }
    case RelationOperation::identity: {
      // Rule (\id\lrule): [e.id] -> { [e] }
      return PartialDNF{{event}};
    }
    case RelationOperation::intersection: {
      // Rule (\cap_2\lrule): [e.(r1 & r2)] -> { [e.r1 & e.r2] }
      CanonicalSet er1 = Set::newSet(operation, event, relation->leftOperand);
      CanonicalSet er2 = Set::newSet(operation, event, relation->rightOperand);
      CanonicalSet er1_and_er2 = Set::newSet(SetOperation::intersection, er1, er2);
      return PartialDNF{{er1_and_er2}};
    }
    case RelationOperation::transitiveClosure: {
      CanonicalSet er = Set::newSet(operation, event, relation->leftOperand);
      CanonicalSet err_star = Set::newSet(operation, er, relation);

      // Rule (\neg*\lrule): ~[e.r*] -> { ~[(e.r)r*], ~[e] }
      // Rule (*\lrule): [e.r*] -> { [(e.r)r*] }, { [e] }
      return context->negated ? PartialDNF{{err_star, event}} : PartialDNF{{err_star}, {event}};
    }
  }
}

// TODO: give better name
PartialDNF substituteHelper2(bool substituteRight, const PartialDNF &disjunction,
                             CanonicalSet otherOperand) {
  std::vector<std::vector<PartialLiteral>> resultDisjunction;
  resultDisjunction.reserve(disjunction.size());
  for (const auto &conjunction : disjunction) {
    std::vector<PartialLiteral> resultConjunction;
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

}  // namespace

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

#if (DEBUG)
  // ------------------ Validation ------------------
  static std::unordered_set<SetOperation> operations = {
      SetOperation::image, SetOperation::domain, SetOperation::singleton, SetOperation::intersection,
      SetOperation::choice, SetOperation::base, SetOperation::full, SetOperation::empty };
  assert(operations.contains(operation));

  const bool isSimple = (left == nullptr && right == nullptr && relation == nullptr);
  const bool hasLabelOrId = (label.has_value() || identifier.has_value());
  switch (operation) {
    case SetOperation::base:
      assert(identifier.has_value() && !label.has_value() && isSimple);
      break;
    case SetOperation::singleton:
      assert (label.has_value() && !identifier.has_value() && isSimple);
      break;
    case SetOperation::empty:
    case SetOperation::full:
      assert (!hasLabelOrId && isSimple);
      break;
    case SetOperation::choice:
    case SetOperation::intersection:
      assert(!hasLabelOrId);
      assert (left != nullptr && right != nullptr && relation == nullptr);
      break;
    case SetOperation::image:
    case SetOperation::domain:
      assert(!hasLabelOrId);
      assert (left != nullptr && relation != nullptr && right == nullptr);
      break;
  }
#endif

  static std::unordered_set<Set> canonicalizer;
  auto [iter, created] = canonicalizer.emplace(operation, left, right, relation, label, identifier);
  return &(*iter);
}

CanonicalSet Set::newSet(SetOperation operation) {
  return newSet(operation, nullptr, nullptr, nullptr, std::nullopt, std::nullopt);
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
  CanonicalSet leftRenamed;
  CanonicalSet rightRenamed;
  switch (operation) {
    case SetOperation::singleton:
      return Set::newEvent(Literal::rename(*label, renaming, inverse));
    case SetOperation::base:
    case SetOperation::empty:
    case SetOperation::full:
      return this;
    case SetOperation::choice:
    case SetOperation::intersection:
      leftRenamed = leftOperand->rename(renaming, inverse);
      rightRenamed = rightOperand->rename(renaming, inverse);
      return Set::newSet(operation, leftRenamed, rightRenamed);
    case SetOperation::image:
    case SetOperation::domain:
      leftRenamed = leftOperand->rename(renaming, inverse);
      return Set::newSet(operation, leftRenamed, relation);
  }
}

std::optional<PartialDNF> Set::applyRule(const Literal *context, bool modalRules) const {
  std::vector<std::vector<PartialLiteral>> result;
  switch (operation) {
    case SetOperation::singleton:
      // no rule applicable to single event constant
      return std::nullopt;
    case SetOperation::empty:
      // Rule (\bot_1):
      return PartialDNF{{BOTTOM}};
    case SetOperation::full: {
      if (context->negated) {
        // Rule (\neg\top_1): this case needs context (handled later)
        return std::nullopt;
      } else {
        // Rule (\top_1): [T] -> { [f] } , only if positive
        CanonicalSet f = Set::newEvent(Set::maxSingletonLabel++);
        return PartialDNF{{f}};
      }
    }
    case SetOperation::choice: {
      // Rule (\neg\cup_1): ~[A | B] -> { ~[A], ~[B] }
      // Rule (\cup_1): [A | B] -> { [A] },{ [B] }
      return context->negated ? PartialDNF{{leftOperand, rightOperand}}
                              : PartialDNF{{leftOperand}, {rightOperand}};
    }
    case SetOperation::intersection:
      if (leftOperand->operation == SetOperation::singleton) {
        CanonicalSet e_and_s = Set::newSet(SetOperation::intersection, leftOperand, rightOperand);
        // Rule (~eL):
        // Rule (eL): [e & s] -> { [e], e.s }
        return context->negated ? PartialDNF{{leftOperand}, {context->substituteSet(e_and_s)}}
                                : PartialDNF{{leftOperand, context->substituteSet(e_and_s)}};
      } else if (rightOperand->operation == SetOperation::singleton) {
        CanonicalSet s_and_e = Set::newSet(SetOperation::intersection, leftOperand, rightOperand);

        // Rule (~eR):
        // Rule (eR): [s & e] -> { [e], s.e }
        return context->negated ? PartialDNF{{rightOperand}, {context->substituteSet(s_and_e)}}
                                : PartialDNF{{rightOperand, context->substituteSet(s_and_e)}};
      } else {
        // [S1 & S2]: apply rules recursively
        auto leftResult = leftOperand->applyRule(context, modalRules);
        if (leftResult) {
          auto disjunction = *leftResult;
          return substituteHelper2(false, disjunction, rightOperand);
        }
        auto rightResult = rightOperand->applyRule(context, modalRules);
        if (rightResult) {
          auto disjunction = *rightResult;
          return substituteHelper2(true, disjunction, leftOperand);
        }
      }

      return std::nullopt;
    case SetOperation::base: {
      if (!context->negated) {
        // Rule (A): [B] -> { [f], f \in B }
        CanonicalSet f = Set::newEvent(Set::maxSingletonLabel++);
        return PartialDNF{{f, Literal(false, *f->label, *identifier)}};
      } else {
        // Rule (~A): requires context (handled later)
        return std::nullopt;
      }
    }
    case SetOperation::image:
    case SetOperation::domain:
      if (leftOperand->operation == SetOperation::singleton) {
        return applyRelationalRule(context, leftOperand, relation, operation, modalRules);
      } else {
        auto setResult = leftOperand->applyRule(context, modalRules);
        if (setResult) {
          for (const auto &cube : *setResult) {
            std::vector<PartialLiteral> newCube;
            for (const auto &partialPredicate : cube) {
              if (std::holds_alternative<Literal>(partialPredicate)) {
                newCube.push_back(partialPredicate);
              } else {
                auto s = std::get<CanonicalSet>(partialPredicate);
                // TODO: there should only be one [] inside each {}
                // otherwise we have to intersect (&) all  []'s after before replacing
                // currently we just assume this is the case without further checking
                CanonicalSet newSet = Set::newSet(operation, s, relation);
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
  CanonicalSet leftSaturated = nullptr;
  CanonicalSet rightSaturated = nullptr;
  if (leftOperand != nullptr) {
    leftSaturated = leftOperand->saturateBase();
  }
  if (rightOperand != nullptr) {
    rightSaturated = rightOperand->saturateBase();
  }
  return Set::newSet(operation, leftSaturated, rightSaturated, relation, label, identifier);
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
