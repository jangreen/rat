#include "Literal.h"

#include <spdlog/spdlog.h>

#include <iostream>

#include "Assumption.h"
#include "utility.h"

namespace {
// ---------------------- Anonymous helper functions ----------------------

std::optional<DNF> handleIntersectionWithEvent(const Literal *context, const CanonicalSet s,
                                               const CanonicalSet e, const RuleDirection direction,
                                               const bool modalRules) {
  // RuleDirection::Left: handle "e & s != 0"
  // RuleDirection::Right: handle "s & e != 0"
  assert(e->operation == SetOperation::singleton);
  // Do case distinction on the shape of "s"
  switch (s->operation) {
    case SetOperation::base:
      // RuleDirection::Left: e & A != 0  ->  e \in A
      // RuleDirection::Right: A & e != 0  ->  e \in A
      return DNF{{Literal(context->negated, *e->label, *s->identifier)}};
    case SetOperation::singleton:
      // RuleDirection::Left: e & f != 0  ->  e == f
      // RuleDirection::Right: f & e != 0  ->  e == f (in both cases use same here)
      // Rule (=)
      return DNF{{Literal(context->negated, *e->label, *s->label)}};
    case SetOperation::empty:
      // RuleDirection::Left: e & 0 != 0  ->  false
      // RuleDirection::Right: 0 & e != 0  ->  false
      return context->negated ? DNF{{TOP}} : DNF{{BOTTOM}};
    case SetOperation::full:
      // RuleDirection::Left: e & 1 != 0  ->  true
      // RuleDirection::Right: 1 & e != 0  ->  true
      return context->negated ? DNF{{BOTTOM}} : DNF{{TOP}};
    case SetOperation::intersection: {
      // RuleDirection::Left: e & (s1 & s2) -> e & s1 , e & s2
      // RuleDirection::Right: (s1 & s2) & e -> s1 & e , s2 & e
      const CanonicalSet e_and_s1 =
          direction == RuleDirection::Left
              ? Set::newSet(SetOperation::intersection, e, s->leftOperand)
              : Set::newSet(SetOperation::intersection, s->leftOperand, e);
      const CanonicalSet e_and_s2 =
          direction == RuleDirection::Left
              ? Set::newSet(SetOperation::intersection, e, s->rightOperand)
              : Set::newSet(SetOperation::intersection, s->rightOperand, e);
      // Rule (~&_1L), Rule (&_1L)
      return context->negated
                 ? DNF{{context->substituteSet(e_and_s1)}, {context->substituteSet(e_and_s2)}}
                 : DNF{{context->substituteSet(e_and_s1), context->substituteSet(e_and_s2)}};
    }
    case SetOperation::choice: {
      return std::nullopt;  // handled in later function
    }
    // -------------- Complex case --------------
    case SetOperation::image:
    case SetOperation::domain: {
      // RuleDirection::Left: e & (s'r)     or      e & (rs')
      // RuleDirection::Right: (s'r) & e    or      (rs') & e
      const CanonicalSet sp = s->leftOperand;
      const CanonicalRelation r = s->relation;
      if (sp->operation != SetOperation::singleton) {
        // RuleDirection::Left: e & s'r -> re & s'     or      e & rs' -> er & s'
        // Rule (._12L)     or      Rule (._21L)
        // RuleDirection::Right: s'r & e -> s' & re        or      rs' & e -> s' & er
        // Rule (._12R)     or      Rule (._21R)
        const SetOperation oppositeOp =
            s->operation == SetOperation::image ? SetOperation::domain : SetOperation::image;
        const CanonicalSet swapped = Set::newSet(oppositeOp, e, r);  // re   or    er
        const CanonicalSet swapped_and_sp =
            direction == RuleDirection::Left ? Set::newSet(SetOperation::intersection, swapped, sp)
                                             : Set::newSet(SetOperation::intersection, sp, swapped);
        return DNF{{context->substituteSet(swapped_and_sp)}};
      } else if (r->operation == RelationOperation::base) {
        // RuleDirection::Left: e & (f.b)     or      e & (b.f)
        // RuleDirection::Right: f.b & e      or      b.f & e
        // shortcut multiple rules
        const std::string b = *r->identifier;
        int first = *e->label;
        int second = *sp->label;
        if (s->operation == SetOperation::image) {
          std::swap(first, second);
        }
        // (first, second) \in b
        return DNF{{Literal(context->negated, first, second, b)}};
      } else {
        // RuleDirection::Left: e & fr     or      e & rf
        // RuleDirection::Right: fr & e      or      rf & e
        // -> r is not base
        // -> apply some rule to fr    or      rf

        const auto sResult = s->applyRule(context, modalRules);
        if (!sResult) {
          // no rule applicable (possible since we omit rules where true is derivable)
          return std::nullopt;
        }

        // RuleDirection::Left: e & sResult
        // RuleDirection::Right: sResult & e
        DNF result;
        result.reserve(sResult->size());
        for (const auto &partialCube : *sResult) {
          Cube cube;
          cube.reserve(partialCube.size());

          for (const auto &partialLiteral : partialCube) {
            if (std::holds_alternative<Literal>(partialLiteral)) {
              auto l = std::get<Literal>(partialLiteral);
              cube.push_back(std::move(l));
            } else {
              const CanonicalSet si = std::get<CanonicalSet>(partialLiteral);
              const CanonicalSet e_and_si = direction == RuleDirection::Left
                                                ? Set::newSet(SetOperation::intersection, e, si)
                                                : Set::newSet(SetOperation::intersection, si, e);
              cube.emplace_back(context->substituteSet(e_and_si));
            }
          }
          result.push_back(cube);
        }
        return result;
      }
    }
  }
  throw std::logic_error("unreachable");
}

}  // namespace

Literal::Literal(bool negated)
    : negated(negated),
      operation(PredicateOperation::constant),
      set(nullptr),
      leftLabel(std::nullopt),
      rightLabel(std::nullopt),
      identifier(std::nullopt),
      saturatedId(0),
      saturatedBase(0) {}

Literal::Literal(const bool negated, const CanonicalSet set)
    : negated(negated),
      operation(PredicateOperation::setNonEmptiness),
      set(set),
      leftLabel(std::nullopt),
      rightLabel(std::nullopt),
      identifier(std::nullopt),
      saturatedId(0),
      saturatedBase(0) {}

Literal::Literal(const bool negated, const int leftLabel, std::string identifier)
    : negated(negated),
      operation(PredicateOperation::set),
      set(nullptr),
      leftLabel(leftLabel),
      rightLabel(std::nullopt),
      identifier(identifier),
      saturatedId(0),
      saturatedBase(0) {}

Literal::Literal(const bool negated, int leftLabel, int rightLabel, std::string identifier)
    : negated(negated),
      operation(PredicateOperation::edge),
      set(nullptr),
      leftLabel(leftLabel),
      rightLabel(rightLabel),
      identifier(identifier),
      saturatedId(0),
      saturatedBase(0) {}

Literal::Literal(const bool negated, int leftLabel, int rightLabel)
    : negated(negated),
      operation(PredicateOperation::equality),
      set(nullptr),
      leftLabel(leftLabel),
      rightLabel(rightLabel),
      identifier(std::nullopt),
      saturatedId(0),
      saturatedBase(0) {}

bool Literal::validate() const {
  switch (operation) {
    case PredicateOperation::constant:
      return set == nullptr && !leftLabel.has_value() && !rightLabel.has_value() &&
             !identifier.has_value();
    case PredicateOperation::edge:
      return set == nullptr && leftLabel.has_value() && rightLabel.has_value() &&
             identifier.has_value();
    case PredicateOperation::equality:
      return set == nullptr && leftLabel.has_value() && rightLabel.has_value() &&
             !identifier.has_value();
    case PredicateOperation::set:
      return set == nullptr && leftLabel.has_value() && !rightLabel.has_value() &&
             identifier.has_value();
      break;
    case PredicateOperation::setNonEmptiness:
      return set != nullptr && !leftLabel.has_value() && !rightLabel.has_value() &&
             !identifier.has_value();
    default:
      return false;
  }
}

int Literal::saturationBoundId = 1;
int Literal::saturationBoundBase = 1;


std::strong_ordering Literal::operator<=>(const Literal &other) const {
  if (auto cmp = (other.negated <=> negated); cmp != 0) return cmp;
  if (auto cmp = operation <=> other.operation; cmp != 0) return cmp;

  // We assume well-formed literals
  switch (operation) {
    case PredicateOperation::edge: {
      auto cmp = *leftLabel <=> *other.leftLabel;
      cmp = (cmp != 0) ? cmp : (*rightLabel <=> *other.rightLabel);
      cmp = (cmp != 0) ? cmp : lexCompare(*identifier, *other.identifier);
      return cmp;
    }
    case PredicateOperation::set: {
      auto cmp = *leftLabel <=> *other.leftLabel;
      cmp = (cmp != 0) ? cmp : lexCompare(*identifier, *other.identifier);
      return cmp;
    }
    case PredicateOperation::equality: {
      auto cmp = *leftLabel <=> *other.leftLabel;
      cmp = (cmp != 0) ? cmp : (*rightLabel <=> *other.rightLabel);
      return cmp;
    }
    case PredicateOperation::setNonEmptiness:
      // NOTE: compare pointer values for very efficient checks, but non-deterministic order
      return set <=> other.set;
    case PredicateOperation::constant:
      return std::strong_ordering::equal; // Equal since we checked signs already
    default:
      assert(false);
      return std::strong_ordering::equal;
  }

}

bool Literal::isNegatedOf(const Literal &other) const {
  return operation == other.operation && negated != other.negated && set == other.set &&
         leftLabel == other.leftLabel && rightLabel == other.rightLabel &&
         identifier == other.identifier;
}

// a literal is normal if it cannot be simplified
bool Literal::isNormal() const {
  switch (operation) {
    case PredicateOperation::setNonEmptiness: {
      if (set->operation == SetOperation::intersection &&
          (set->leftOperand->operation == SetOperation::singleton ||
           set->rightOperand->operation == SetOperation::singleton)) {
        return false;
      }
      return set->isNormal();
    }
    case PredicateOperation::constant:
      return false;
    case PredicateOperation::equality:
      return negated && rightLabel != leftLabel;
    case PredicateOperation::set:
    case PredicateOperation::edge:
      return true;
    default:
      throw std::logic_error("unreachable");
  }
}

bool Literal::hasTopSet() const {
  if (operation == PredicateOperation::setNonEmptiness) {
    return set->hasTopSet();
  }
  return false;
}

bool Literal::isPositiveEdgePredicate() const {
  return !negated && operation == PredicateOperation::edge;
}

bool Literal::isPositiveEqualityPredicate() const {
  return !negated && operation == PredicateOperation::equality;
}

std::vector<int> Literal::labels() const {
  switch (operation) {
    case PredicateOperation::constant:
      return {};
    case PredicateOperation::setNonEmptiness: {
      return set->getLabels();
    }
    case PredicateOperation::edge:
    case PredicateOperation::equality: {
      return {*leftLabel, *rightLabel};
    }
    case PredicateOperation::set: {
      return {*leftLabel};
    }
    default:
      throw std::logic_error("unreachable");
  }
}

std::vector<CanonicalSet> Literal::labelBaseCombinations() const {
  switch (operation) {
    case PredicateOperation::constant:
    case PredicateOperation::equality:
    case PredicateOperation::set:
      return {};
    case PredicateOperation::setNonEmptiness: {
      return set->getLabelBaseCombinations();
    }
    case PredicateOperation::edge: {
      // (e1,e2) \in b
      const CanonicalSet e1 = Set::newEvent(*leftLabel);
      const CanonicalSet e2 = Set::newEvent(*rightLabel);
      const CanonicalRelation b = Relation::newBaseRelation(*identifier);

      const CanonicalSet e1b = Set::newSet(SetOperation::image, e1, b);
      const CanonicalSet be2 = Set::newSet(SetOperation::domain, e2, b);
      return {e1b, be2};
    }
    default:
      throw std::logic_error("unreachable");
  }
}

DNF toDNF(const Literal *context, const PartialDNF &partialDNF) {
  DNF result;
  result.reserve(partialDNF.size());
  for (const auto &partialCube : partialDNF) {
    Cube cube;
    cube.reserve(partialCube.size());

    for (const auto &partialLiteral : partialCube) {
      if (std::holds_alternative<Literal>(partialLiteral)) {
        auto l = std::get<Literal>(partialLiteral);
        cube.push_back(std::move(l));
      } else {
        const CanonicalSet s = std::get<CanonicalSet>(partialLiteral);
        cube.push_back(context->substituteSet(s));
      }
    }
    result.push_back(cube);
  }
  return result;
}

std::optional<DNF> Literal::applyRule(const bool modalRules) const {
  switch (operation) {
    case PredicateOperation::edge:
    case PredicateOperation::constant:
    case PredicateOperation::set: {
      return std::nullopt;  // no rule applicable
    }
    case PredicateOperation::equality: {
      // (\neg=): ~(e = e) -> FALSE
      if (leftLabel == rightLabel) {
        return negated ? DNF{{BOTTOM}} : DNF{{TOP}};
      }
      return std::nullopt;  // no rule applicable in case e1 = e2
    }
    case PredicateOperation::setNonEmptiness:
      if (set->operation == SetOperation::intersection) {
        // s1 & s2 != 0
        if (set->leftOperand->operation == SetOperation::singleton) {
          return handleIntersectionWithEvent(this, set->rightOperand, set->leftOperand,
                                             RuleDirection::Left, modalRules);
        } else if (set->rightOperand->operation == SetOperation::singleton) {
          return handleIntersectionWithEvent(this, set->leftOperand, set->rightOperand,
                                             RuleDirection::Right, modalRules);
        }
      }
      // apply non-root rules
      if (const auto result = set->applyRule(this, modalRules)) {
        return toDNF(this, *result);
      }
      return std::nullopt;
  }
  throw std::logic_error("unreachable");
}

bool Literal::substitute(const CanonicalSet search, const CanonicalSet replace, int n) {
  if (operation != PredicateOperation::setNonEmptiness) {
    // only substitute in set expressions
    return false;
  }

  if (const auto newSet = set->substitute(search, replace, &n); newSet != set) {
    set = newSet;
    return true;
  }
  return false;
}

Literal Literal::substituteSet(const CanonicalSet set) const {
  assert(operation == PredicateOperation::setNonEmptiness);
  Literal l(true, set);
  l.negated = negated;
  l.saturatedBase = saturatedBase;
  l.saturatedId = saturatedId;
  return l;
}

void Literal::rename(const Renaming &renaming) {
  switch (operation) {
    case PredicateOperation::constant:
      return;
    case PredicateOperation::setNonEmptiness: {
      set = set->rename(renaming);
      return;
    }
    case PredicateOperation::edge:
    case PredicateOperation::equality: {
      leftLabel = renaming.rename(*leftLabel);
      rightLabel = renaming.rename(*rightLabel);
      return;
    }
    case PredicateOperation::set: {
      leftLabel = renaming.rename(*leftLabel);
      return;
    }
  }
}

void Literal::saturateBase() {
  if (!negated || saturatedBase >= saturationBoundBase) {
    return;
  }
  switch (operation) {
    case PredicateOperation::edge:
      if (Assumption::baseAssumptions.contains(*identifier)) {
        const auto assumption = Assumption::baseAssumptions.at(*identifier);
        // (e1, e2) \in b, R <= b
        const CanonicalSet e1 = Set::newEvent(*leftLabel);
        const CanonicalSet e2 = Set::newEvent(*rightLabel);
        const CanonicalSet e1R = Set::newSet(SetOperation::image, e1, assumption.relation);
        const CanonicalSet e1R_and_e2 = Set::newSet(SetOperation::intersection, e1R, e2);

        operation = PredicateOperation::setNonEmptiness;
        set = e1R_and_e2;
        leftLabel = std::nullopt;
        rightLabel = std::nullopt;
        identifier = std::nullopt;
        saturatedBase++;
      }
      return;
    case PredicateOperation::setNonEmptiness: {
      if (const auto saturatedSet = set->saturateBase(); set != saturatedSet) {
        set = saturatedSet;
        saturatedBase++;
      }
      return;
    }
    default:
      return;
  }
}
void Literal::saturateId() {
  if (!negated || saturatedId >= saturationBoundId) {
    return;
  }
  switch (operation) {
    case PredicateOperation::edge: {
      // (e1, e2) \in b, R <= id
      const CanonicalSet e1 = Set::newEvent(*leftLabel);
      const CanonicalSet e2 = Set::newEvent(*rightLabel);
      const CanonicalRelation b = Relation::newBaseRelation(*identifier);
      const CanonicalSet e1R = Set::newSet(SetOperation::image, e1, Assumption::masterIdRelation());
      const CanonicalSet be2 = Set::newSet(SetOperation::domain, e2, b);
      const CanonicalSet e1R_and_be2 = Set::newSet(SetOperation::intersection, e1R, be2);

      operation = PredicateOperation::setNonEmptiness;
      set = e1R_and_be2;
      leftLabel = std::nullopt;
      rightLabel = std::nullopt;
      identifier = std::nullopt;
      saturatedId++;
      return;
    }
    case PredicateOperation::setNonEmptiness: {
      return;
      /* FIXME: Is this test code???
      auto saturatedSet = set->saturateId();
      if (set != saturatedSet) {
        set = saturatedSet;
        saturatedId++;
      }
      return;
      */
    }
    default:
      return;
  }
}

std::string Literal::toString() const {
  std::string output;
  if (negated && PredicateOperation::constant != operation) {
    output += "~";
  }
  switch (operation) {
    case PredicateOperation::constant:
      output += negated ? "FALSE" : "TRUE";
      break;
    case PredicateOperation::edge:
      output +=
          *identifier + "(" + std::to_string(*leftLabel) + "," + std::to_string(*rightLabel) + ")";
      break;
    case PredicateOperation::set:
      output += *identifier + "(" + std::to_string(*leftLabel) + ")";
      break;
    case PredicateOperation::equality:
      output += std::to_string(*leftLabel) + " = " + std::to_string(*rightLabel);
      break;
    case PredicateOperation::setNonEmptiness:
      output += set->toString();
      break;
  }
  return output;
}
