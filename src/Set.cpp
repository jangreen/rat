#include <spdlog/spdlog.h>

#include <cassert>
#include <unordered_set>

#include "Assumption.h"
#include "Literal.h"

namespace {
// ---------------------- Anonymous helper functions ----------------------

std::optional<PartialDNF> applyRelationalRule(const Literal *context, CanonicalSet event,
                                              const CanonicalRelation relation,
                                              const SetOperation operation, const bool modalRules) {
  // operation indicates if left or right rule is used
  assert(operation == SetOperation::image || operation == SetOperation::domain);

  switch (relation->operation) {
    case RelationOperation::base: {
      // only use if want to
      if (!modalRules) {
        return std::nullopt;
      }
      if (context->negated) {
        // Rule (~aL), Rule (~aR): requires context (handled later)
        return std::nullopt;
      }

      // RuleDirection::Left: [e.b] -> { [f], (e,f) \in b }
      // -> Rule (aL)
      // RuleDirection::Right: [b.e] -> { [f], (f,e) \in b }
      // -> Rule (aR)
      const CanonicalSet f = Set::newEvent(Set::maxSingletonLabel++);
      const auto &b = *relation->identifier;
      const int first = operation == SetOperation::image ? *event->label : *f->label;
      const int second = operation == SetOperation::image ? *f->label : *event->label;

      return PartialDNF{{f, Literal(false, first, second, b)}};
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
      const CanonicalSet er1 = Set::newSet(operation, event, relation->leftOperand);
      const CanonicalSet er2 = Set::newSet(operation, event, relation->rightOperand);
      return context->negated ? PartialDNF{{er1, er2}} : PartialDNF{{er1}, {er2}};
    }
    case RelationOperation::composition: {
      // Rule (._22L): [e(a.b)] -> { [(e.a)b] }
      CanonicalRelation first =
          operation == SetOperation::image ? relation->leftOperand : relation->rightOperand;
      CanonicalRelation second =
          operation == SetOperation::image ? relation->rightOperand : relation->leftOperand;
      const CanonicalSet ea = Set::newSet(operation, event, first);
      const CanonicalSet ea_b = Set::newSet(operation, ea, second);
      return PartialDNF{{ea_b}};
    }
    case RelationOperation::converse: {
      // Rule (^-1L): [e.(r^-1)] -> { [r.e] }
      const SetOperation oppositOp =
          operation == SetOperation::image ? SetOperation::domain : SetOperation::image;
      const CanonicalSet re = Set::newSet(oppositOp, event, relation->leftOperand);
      return PartialDNF{{re}};
    }
    case RelationOperation::empty: {
      // Rule (\bot_2L), (\bot_2R) + (\bot_1) -> FALSE
      return context->negated ? PartialDNF{{TOP}} : PartialDNF{{BOTTOM}};
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
      const CanonicalSet er1 = Set::newSet(operation, event, relation->leftOperand);
      const CanonicalSet er2 = Set::newSet(operation, event, relation->rightOperand);
      const CanonicalSet er1_and_er2 = Set::newSet(SetOperation::intersection, er1, er2);
      return PartialDNF{{er1_and_er2}};
    }
    case RelationOperation::transitiveClosure: {
      // Rule (\neg*\lrule): ~[e.r*] -> { ~[(e.r)r*], ~[e] }
      // Rule (*\lrule): [e.r*] -> { [(e.r)r*] }, { [e] }
      const CanonicalSet er = Set::newSet(operation, event, relation->leftOperand);
      const CanonicalSet err_star = Set::newSet(operation, er, relation);
      return context->negated ? PartialDNF{{err_star, event}} : PartialDNF{{err_star}, {event}};
    }
  }
  throw std::logic_error("unreachable");
}

// TODO: give better name
PartialDNF substituteHelper2(const bool substituteRight, const PartialDNF &disjunction,
                             const CanonicalSet otherOperand) {
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

// initialization helper
bool calcIsNormal(const SetOperation operation, const CanonicalSet leftOperand,
                  const CanonicalSet rightOperand, const CanonicalRelation relation) {
  switch (operation) {
    case SetOperation::choice:
    case SetOperation::intersection:
      assert(rightOperand != nullptr);
      return leftOperand->isNormal() && rightOperand->isNormal();
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
                 : leftOperand->isNormal();
    default:
      throw std::logic_error("unreachable");
  }
}

std::vector<int> calcLabels(const SetOperation operation, const CanonicalSet leftOperand,
                            const CanonicalSet rightOperand, const std::optional<int> label) {
  switch (operation) {
    case SetOperation::intersection:
    case SetOperation::choice: {
      auto leftLabels = leftOperand->getLabels();
      auto rightLabels = rightOperand->getLabels();
      leftLabels.insert(std::end(leftLabels), std::begin(rightLabels), std::end(rightLabels));
      return leftLabels;
    }
    case SetOperation::domain:
    case SetOperation::image:
      return leftOperand->getLabels();
    case SetOperation::singleton:
      return {*label};
    default:
      return {};
  }
}

std::vector<CanonicalSet> calcLabelBaseCombinations(const SetOperation operation,
                                                    const CanonicalSet leftOperand,
                                                    const CanonicalSet rightOperand,
                                                    const CanonicalRelation relation,
                                                    const CanonicalSet thisRef) {
  switch (operation) {
    case SetOperation::choice:
    case SetOperation::intersection: {
      auto left = leftOperand->getLabelBaseCombinations();
      auto right = rightOperand->getLabelBaseCombinations();
      left.insert(std::end(left), std::begin(right), std::end(right));
      return left;
    }
    case SetOperation::domain:
    case SetOperation::image: {
      return leftOperand->operation == SetOperation::singleton &&
                     relation->operation == RelationOperation::base
                 ? std::vector{thisRef}
                 : leftOperand->getLabelBaseCombinations();
    }
    default:
      return {};
  }
}

bool calcHasTopSet(const SetOperation operation, const CanonicalSet leftOperand,
                   const CanonicalSet rightOperand) {
  return SetOperation::full == operation || (leftOperand != nullptr && leftOperand->hasTopSet()) ||
         (rightOperand != nullptr && rightOperand->hasTopSet());
}

}  // namespace

int Set::maxSingletonLabel = 0;

void Set::completeInitialization() const {
  this->_isNormal = calcIsNormal(operation, leftOperand, rightOperand, relation);
  this->_hasTopSet = calcHasTopSet(operation, leftOperand, rightOperand);
  this->labels = calcLabels(operation, leftOperand, rightOperand, label);
  this->labelBaseCombinations =
      calcLabelBaseCombinations(operation, leftOperand, rightOperand, relation, this);
}

const bool &Set::isNormal() const { return _isNormal; }

const bool &Set::hasTopSet() const { return _hasTopSet; }

const std::vector<int> &Set::getLabels() const { return labels; }

const std::vector<CanonicalSet> &Set::getLabelBaseCombinations() const {
  return labelBaseCombinations;
}

Set::Set(const SetOperation operation, const CanonicalSet left, const CanonicalSet right,
         const CanonicalRelation relation, const std::optional<int> label,
         std::optional<std::string> identifier)
    : operation(operation),
      identifier(std::move(identifier)),
      label(label),
      leftOperand(left),
      rightOperand(right),
      relation(relation) {}

CanonicalSet Set::newSet(const SetOperation operation, const CanonicalSet left,
                         const CanonicalSet right, const CanonicalRelation relation,
                         const std::optional<int> label,
                         const std::optional<std::string> &identifier) {
#if (DEBUG)
  // ------------------ Validation ------------------
  static std::unordered_set operations = {SetOperation::image,     SetOperation::domain,
                                          SetOperation::singleton, SetOperation::intersection,
                                          SetOperation::choice,    SetOperation::base,
                                          SetOperation::full,      SetOperation::empty};
  assert(operations.contains(operation));

  const bool isSimple = (left == nullptr && right == nullptr && relation == nullptr);
  const bool hasLabelOrId = (label.has_value() || identifier.has_value());
  switch (operation) {
    case SetOperation::base:
      assert(identifier.has_value() && !label.has_value() && isSimple);
      break;
    case SetOperation::singleton:
      assert(label.has_value() && !identifier.has_value() && isSimple);
      break;
    case SetOperation::empty:
    case SetOperation::full:
      assert(!hasLabelOrId && isSimple);
      break;
    case SetOperation::choice:
    case SetOperation::intersection:
      assert(!hasLabelOrId);
      assert(left != nullptr && right != nullptr && relation == nullptr);
      break;
    case SetOperation::image:
    case SetOperation::domain:
      assert(!hasLabelOrId);
      assert(left != nullptr && relation != nullptr && right == nullptr);
      break;
  }
#endif
  static std::unordered_set<Set> canonicalizer;
  auto [iter, created] = canonicalizer.emplace(operation, left, right, relation, label, identifier);
  if (created) {
    iter->completeInitialization();
  }
  return &*iter;
}

CanonicalSet Set::newSet(const SetOperation operation) {
  return newSet(operation, nullptr, nullptr, nullptr, std::nullopt, std::nullopt);
}
CanonicalSet Set::newSet(const SetOperation operation, const CanonicalSet left,
                         const CanonicalSet right) {
  return newSet(operation, left, right, nullptr, std::nullopt, std::nullopt);
}

CanonicalSet Set::newSet(const SetOperation operation, const CanonicalSet left,
                         const CanonicalRelation relation) {
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

CanonicalSet Set::substitute(const CanonicalSet search, const CanonicalSet replace, int *n) const {
  if (this == search) {
    if (*n == 1 || *n == -1) {
      return replace;
    }
    (*n)--;
    return this;
  }
  if (leftOperand != nullptr) {
    const auto leftSub = leftOperand->substitute(search, replace, n);
    if (leftSub != leftOperand) {
      return newSet(operation, leftSub, rightOperand, relation, label, identifier);
    }
  }
  if (rightOperand != nullptr) {
    const auto rightSub = rightOperand->substitute(search, replace, n);
    if (rightSub != rightOperand) {
      return newSet(operation, leftOperand, rightSub, relation, label, identifier);
    }
  }
  return this;
}

CanonicalSet Set::rename(const Renaming &renaming) const {
  CanonicalSet leftRenamed;
  CanonicalSet rightRenamed;
  switch (operation) {
    case SetOperation::singleton:
      return newEvent(renaming.rename(*label));
    case SetOperation::base:
    case SetOperation::empty:
    case SetOperation::full:
      return this;
    case SetOperation::choice:
    case SetOperation::intersection:
      leftRenamed = leftOperand->rename(renaming);
      rightRenamed = rightOperand->rename(renaming);
      return newSet(operation, leftRenamed, rightRenamed);
    case SetOperation::image:
    case SetOperation::domain:
      leftRenamed = leftOperand->rename(renaming);
      return newSet(operation, leftRenamed, relation);
  }
  throw std::logic_error("unreachable");
}

std::optional<PartialDNF> Set::applyRule(const Literal *context, const bool modalRules) const {
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
      }
      // Rule (\top_1): [T] -> { [f] } , only if positive
      const CanonicalSet f = newEvent(maxSingletonLabel++);
      return PartialDNF{{f}};
    }
    case SetOperation::choice: {
      // Rule (\neg\cup_1): ~[A | B] -> { ~[A], ~[B] }
      // Rule (\cup_1): [A | B] -> { [A] },{ [B] }
      return context->negated ? PartialDNF{{leftOperand, rightOperand}}
                              : PartialDNF{{leftOperand}, {rightOperand}};
    }
    case SetOperation::intersection: {
      if (leftOperand->operation != SetOperation::singleton &&
          rightOperand->operation != SetOperation::singleton) {
        // [S1 & S2]: apply rules recursively
        if (const auto leftResult = leftOperand->applyRule(context, modalRules)) {
          const auto &disjunction = *leftResult;
          return substituteHelper2(false, disjunction, rightOperand);
        }
        if (const auto rightResult = rightOperand->applyRule(context, modalRules)) {
          const auto &disjunction = *rightResult;
          return substituteHelper2(true, disjunction, leftOperand);
        }
        return std::nullopt;
      }

      // Case [e & s] OR [s & e]
      // Rule (~eL) / (~eR):
      // Rule (eL): [e & s] -> { [e], e.s }
      // Rule (eR): [s & e] -> { [e], s.e }
      const CanonicalSet intersection =
          newSet(SetOperation::intersection, leftOperand, rightOperand);
      const CanonicalSet &singleton =
          leftOperand->operation == SetOperation::singleton ? leftOperand : rightOperand;
      const Literal substitute = context->substituteSet(intersection);
      return context->negated ? PartialDNF{{singleton}, {substitute}}
                              : PartialDNF{{singleton, substitute}};
    }
    case SetOperation::base: {
      if (context->negated) {
        // Rule (~A): requires context (handled later)
        return std::nullopt;
      }
      // Rule (A): [B] -> { [f], f \in B }
      const CanonicalSet f = newEvent(maxSingletonLabel++);
      return PartialDNF{{f, Literal(false, *f->label, *identifier)}};
    }
    case SetOperation::image:
    case SetOperation::domain:
      if (leftOperand->operation == SetOperation::singleton) {
        return applyRelationalRule(context, leftOperand, relation, operation, modalRules);
      }

      const auto setResult = leftOperand->applyRule(context, modalRules);
      if (!setResult) {
        return std::nullopt;
      }

      std::vector<std::vector<PartialLiteral>> result;
      result.reserve(setResult->size());
      for (const auto &cube : *setResult) {
        std::vector<PartialLiteral> newCube;
        newCube.reserve(cube.size());
        for (const auto &partialPredicate : cube) {
          if (std::holds_alternative<Literal>(partialPredicate)) {
            newCube.push_back(partialPredicate);
          } else {
            const auto &s = std::get<CanonicalSet>(partialPredicate);
            // TODO: there should only be one [] inside each {}
            // otherwise we have to intersect (&) all  []'s after before replacing
            // currently we just assume this is the case without further checking
            const CanonicalSet newSet = Set::newSet(operation, s, relation);
            newCube.emplace_back(newSet);
          }
        }
        result.push_back(newCube);
      }
      return result;
  }
  throw std::logic_error("unreachable");
}

CanonicalSet Set::saturateBase() const {
  if (operation == SetOperation::domain || operation == SetOperation::image) {
    assert(leftOperand != nullptr);

    if (leftOperand->operation != SetOperation::singleton) {
      const auto leftSaturated = leftOperand->saturateBase();
      return (leftOperand == leftSaturated)
                 ? this
                 : newSet(operation, leftSaturated, rightOperand, relation, label, identifier);
    }

    if (relation->operation == RelationOperation::base) {
      const auto &relationName = *relation->identifier;
      if (Assumption::baseAssumptions.contains(relationName)) {
        const auto &assumption = Assumption::baseAssumptions.at(relationName);
        const auto r =
            Relation::newRelation(RelationOperation::choice, relation, assumption.relation);
        return newSet(operation, leftOperand, r);
      }
      return this;
    }
    /* TODO: I think we can merge both ifs to "if left==Singleton && right == base"
        and otherwise fallthrough to the recursive case.
    */
    // NOTE: Reachable if left==Singleton and right != base
  }

  const CanonicalSet leftSaturated = leftOperand != nullptr ? leftOperand->saturateBase() : nullptr;
  const CanonicalSet rightSaturated =
      rightOperand != nullptr ? rightOperand->saturateBase() : nullptr;
  return newSet(operation, leftSaturated, rightSaturated, relation, label, identifier);
}

CanonicalSet Set::saturateId() const {
  if (operation == SetOperation::domain || operation == SetOperation::image) {
    assert(leftOperand != nullptr);

    if (leftOperand->operation != SetOperation::singleton) {
      const auto leftSaturated = leftOperand->saturateId();
      return (leftOperand == leftSaturated)
                 ? this
                 : newSet(operation, leftSaturated, rightOperand, relation, label, identifier);
    }

    if (relation->operation == RelationOperation::base) {
      // e.b -> (e.R).b
      const auto eR = newSet(SetOperation::image, leftOperand, Assumption::masterIdRelation());
      const auto &b = relation;
      return newSet(operation, eR, b);
    }
  }

  const CanonicalSet leftSaturated = leftOperand != nullptr ? leftOperand->saturateId() : nullptr;
  const CanonicalSet rightSaturated =
      rightOperand != nullptr ? rightOperand->saturateId() : nullptr;
  return newSet(operation, leftSaturated, rightSaturated, relation, label, identifier);
}

std::string Set::toString() const {
  if (cachedStringRepr) {
    return *cachedStringRepr;
  }
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
  cachedStringRepr.emplace(std::move(output));
  return *cachedStringRepr;
}
