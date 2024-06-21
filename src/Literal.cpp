#include "Literal.h"

#include <spdlog/spdlog.h>

#include <iostream>

namespace {
// ---------------------- Anonymous helper functions ----------------------

std::optional<DNF> handleIntersectionWithEvent(CanonicalSet s, CanonicalSet e,
                                               RuleDirection direction, bool negated,
                                               bool modalRules) {
  // RuleDirection::Left: handle "e & s != 0"
  // RuleDirection::Right: handle "s & e != 0"
  assert(e->operation == SetOperation::singleton);
  // Do case distinction on the shape of "s"
  switch (s->operation) {
    case SetOperation::base:
      // RuleDirection::Left: e & A != 0  ->  e \in A
      // RuleDirection::Right: A & e != 0  ->  e \in A
      return DNF{{Literal(negated, *e->label, *s->identifier)}};
    case SetOperation::singleton:
      // RuleDirection::Left: e & f != 0  ->  e == f
      // RuleDirection::Right: f & e != 0  ->  e == f (in both cases use same here)
      // Rule (=)
      return DNF{{Literal(negated, *e->label, *s->label)}};
    case SetOperation::empty:
      // RuleDirection::Left: e & 0 != 0  ->  false
      // RuleDirection::Right: 0 & e != 0  ->  false
      return DNF{{BOTTOM}};
    case SetOperation::full:
      // RuleDirection::Left: e & 1 != 0  ->  true
      // RuleDirection::Right: 1 & e != 0  ->  true
      return std::nullopt;  // do nothing
    case SetOperation::intersection: {
      // RuleDirection::Left: e & (s1 & s2) -> e & s1 , e & s2
      // RuleDirection::Right: (s1 & s2) & e -> s1 & e , s2 & e
      CanonicalSet e_and_s1 = direction == RuleDirection::Left
                                  ? Set::newSet(SetOperation::intersection, e, s->leftOperand)
                                  : Set::newSet(SetOperation::intersection, s->leftOperand, e);
      CanonicalSet e_and_s2 = direction == RuleDirection::Left
                                  ? Set::newSet(SetOperation::intersection, e, s->rightOperand)
                                  : Set::newSet(SetOperation::intersection, s->rightOperand, e);

      if (negated) {
        // Rule (~&_1L)
        return DNF{{Literal(negated, e_and_s1), Literal(negated, e_and_s2)}};
      } else {
        // Rule (&_1L)
        return DNF{{Literal(negated, e_and_s1)}, {Literal(negated, e_and_s2)}};
      }
      assert(false);  // Unreachable
    }
    case SetOperation::choice: {
      // FIXME: Is this dual to the intersection rule? If so, the code can be merged easily
      // e & (s1 | s2) != 0  ->  (e & s1) | (e & s2) ???
      assert(false);  // Not implemented
    }
    // -------------- Complex case --------------
    case SetOperation::image:
    case SetOperation::domain: {
      // RuleDirection::Left: e & (s'r)     or      e & (rs')
      // RuleDirection::Right: (s'r) & e    or      (rs') & e
      CanonicalSet sp = s->leftOperand;
      CanonicalRelation r = s->relation;
      if (sp->operation != SetOperation::singleton) {
        // RuleDirection::Left: e & s'r -> re & s'     or      e & rs' -> er & s'
        // Rule (._12L)     or      Rule (._21L)
        // RuleDirection::Right: s'r & e -> s' & re        or      rs' & e -> s' & er
        // Rule (._12R)     or      Rule (._21R)
        const SetOperation oppositeOp =
            s->operation == SetOperation::image ? SetOperation::domain : SetOperation::image;
        CanonicalSet swapped = Set::newSet(oppositeOp, e, r);  // re   or    er
        CanonicalSet swapped_and_sp = direction == RuleDirection::Left
                                          ? Set::newSet(SetOperation::intersection, swapped, sp)
                                          : Set::newSet(SetOperation::intersection, sp, swapped);
        return DNF{{Literal(negated, swapped_and_sp)}};
      } else if (r->operation == RelationOperation::base) {
        // RuleDirection::Left: e & (f.b)     or      e & (b.f)
        // RuleDirection::Right: f.b & e      or      b.f & e
        // shortcut multiple rules
        std::string b = *r->identifier;
        int first = *e->label;
        int second = *sp->label;
        if (s->operation == SetOperation::image) {
          std::swap(first, second);
        }
        if (direction == RuleDirection::Right) {
          std::swap(first, second);
        }
        // (first, second) \in b
        return DNF{{Literal(negated, first, second, b)}};
      } else {
        // RuleDirection::Left: e & fr     or      e & rf
        // RuleDirection::Right: fr & e      or      rf & e
        // -> r is not base
        // -> apply some rule to fr    or      rf

        auto sResult = s->applyRule(negated, modalRules);
        if (!sResult) {
          assert(false);  // Unreachable
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
              Literal l = std::get<Literal>(partialLiteral);
              cube.push_back(std::move(l));
            } else {
              CanonicalSet si = std::get<CanonicalSet>(partialLiteral);
              CanonicalSet e_and_si = direction == RuleDirection::Left
                                          ? Set::newSet(SetOperation::intersection, e, si)
                                          : Set::newSet(SetOperation::intersection, si, e);
              cube.emplace_back(negated, e_and_si);
            }
          }
          result.push_back(cube);
        }
        return result;
      }
    }
  }
  assert(false);
}
}  // namespace

Literal::Literal(bool negated, CanonicalSet set)
    : negated(negated), operation(PredicateOperation::setNonEmptiness), set(set) {}

Literal::Literal(bool negated, int leftLabel, std::string identifier)
    : negated(negated),
      operation(PredicateOperation::set),
      leftLabel(leftLabel),
      set(nullptr),
      identifier(identifier) {}

Literal::Literal(bool negated, int leftLabel, int rightLabel, std::string identifier)
    : negated(negated),
      operation(PredicateOperation::edge),
      leftLabel(leftLabel),
      rightLabel(rightLabel),
      set(nullptr),
      identifier(identifier) {}

Literal::Literal(bool negated, int leftLabel, int rightLabel)
    : negated(negated),
      operation(PredicateOperation::equality),
      leftLabel(leftLabel),
      rightLabel(rightLabel),
      set(nullptr) {}

bool Literal::operator==(const Literal &other) const {
  return operation == other.operation && negated == other.negated && set == other.set &&
         identifier == other.identifier;
}

bool Literal::operator<(const Literal &other) const {
  // TODO (TH): toString must traverse the whole DAG to obtain the string.
  //  If you use this in any performance critical part, you should change this.
  // sort lexicographically
  return toString() < other.toString();
}

bool Literal::isNormal() const {
  if (operation == PredicateOperation::setNonEmptiness) {
    return set->isNormal;
  }
  return true;
}

bool Literal::hasTopSet() const {
  if (operation == PredicateOperation::setNonEmptiness) {
    return set->hasTopSet;
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
    case PredicateOperation::setNonEmptiness: {
      return set->labels;
    }
    case PredicateOperation::edge:
    case PredicateOperation::equality: {
      return {*leftLabel, *rightLabel};
    }
    case PredicateOperation::set: {
      return {*leftLabel};
    }
    default:
      assert(false);
  }
}

std::vector<CanonicalSet> Literal::labelBaseCombinations() const {
  switch (operation) {
    case PredicateOperation::setNonEmptiness: {
      return set->labelBaseCombinations;
    }
    case PredicateOperation::edge: {
      // (e1,e2) \in b
      CanonicalSet e1 = Set::newEvent(*leftLabel);
      CanonicalSet e2 = Set::newEvent(*rightLabel);
      CanonicalRelation b = Relation::newBaseRelation(*identifier);

      CanonicalSet e1b = Set::newSet(SetOperation::image, e1, b);
      CanonicalSet be2 = Set::newSet(SetOperation::domain, e2, b);
      return {e1b, be2};
    }
    default:
      return {};
  }
}

DNF toDNF(bool negated, const std::vector<std::vector<PartialPredicate>> &partialDNF) {
  DNF result;
  result.reserve(partialDNF.size());
  for (const auto &partialCube : partialDNF) {
    Cube cube;
    cube.reserve(partialCube.size());

    for (const auto &partialLiteral : partialCube) {
      if (std::holds_alternative<Literal>(partialLiteral)) {
        Literal l = std::get<Literal>(partialLiteral);
        cube.push_back(std::move(l));
      } else {
        CanonicalSet s = std::get<CanonicalSet>(partialLiteral);
        Literal l(negated, s);
        cube.push_back(std::move(l));
      }
    }
    result.push_back(cube);
  }
  return result;
}

std::optional<DNF> Literal::applyRule(bool modalRules) {
  switch (operation) {
    case PredicateOperation::edge:
    case PredicateOperation::set: {
      return std::nullopt;  // no rule applicable
    }
    case PredicateOperation::equality: {
      if (negated && *leftLabel == *rightLabel) {
        // (\neg=): ~(e1 = e2) -> FALSE
        DNF result = {{BOTTOM}};
        return result;
      }
      return std::nullopt;  // no rule applicable in case e1 = e2
    }
    case PredicateOperation::setNonEmptiness:
      if (set->operation == SetOperation::intersection) {
        // s1 & s2 != 0
        if (set->leftOperand->operation == SetOperation::singleton) {
          return handleIntersectionWithEvent(set->rightOperand, set->leftOperand,
                                             RuleDirection::Left, negated, modalRules);
        } else if (set->rightOperand->operation == SetOperation::singleton) {
          return handleIntersectionWithEvent(set->leftOperand, set->rightOperand,
                                             RuleDirection::Right, negated, modalRules);
        }
      }
      // apply non-root rules
      auto result = set->applyRule(negated, modalRules);
      if (result) {
        return toDNF(negated, *result);
      }
      return std::nullopt;
  }
  assert(false);  // FIXME: REACHABLE!!!
}

bool Literal::substitute(CanonicalSet search, CanonicalSet replace, int n) {
  switch (operation) {
    case PredicateOperation::setNonEmptiness: {
      auto newSet = set->substitute(search, replace, &n);
      if (newSet != set) {
        set = newSet;
        return true;
      }
      return false;
    }
    default:
      spdlog::error("TODO");
      exit(0);
  }
}

void Literal::rename(const Renaming &renaming, bool inverse) {
  switch (operation) {
    case PredicateOperation::setNonEmptiness: {
      set = set->rename(renaming, inverse);
      return;
    }
    default:
      spdlog::error("TODO");
      exit(0);
  }
}

void Literal::saturateBase() {
  if (!negated) {
    return;
  }
  switch (operation) {
    case PredicateOperation::edge:
      if (Assumption::baseAssumptions.contains(*identifier)) {
        auto assumption = Assumption::baseAssumptions.at(*identifier);
        // (e1, e2) \in b, R <= b
        CanonicalSet e1 = Set::newEvent(*leftLabel);
        CanonicalSet e2 = Set::newEvent(*rightLabel);
        CanonicalSet e1R = Set::newSet(SetOperation::image, e1, assumption.relation);
        CanonicalSet e1R_and_e2 = Set::newSet(SetOperation::intersection, e1R, e2);

        operation = PredicateOperation::setNonEmptiness;
        set = e1R_and_e2;
        identifier = std::nullopt;
      }
      return;
    case PredicateOperation::setNonEmptiness:
      set = set->saturateBase();
      return;
    default:
      return;
  }
}
void Literal::saturateId() {
  if (!negated) {
    return;
  }
  switch (operation) {
    case PredicateOperation::edge: {
      // (e1, e2) \in b, R <= id
      CanonicalSet e1 = Set::newEvent(*leftLabel);
      CanonicalSet e2 = Set::newEvent(*rightLabel);
      CanonicalRelation b = Relation::newBaseRelation(*identifier);
      CanonicalSet e1R = Set::newSet(SetOperation::image, e1, Assumption::masterIdRelation());
      CanonicalSet be2 = Set::newSet(SetOperation::domain, e2, b);
      CanonicalSet e1R_and_be2 = Set::newSet(SetOperation::intersection, e1R, be2);

      operation = PredicateOperation::setNonEmptiness;
      set = e1R_and_be2;
      identifier = std::nullopt;
      return;
    }
    case PredicateOperation::setNonEmptiness:
      set = set->saturateId();
      return;
    default:
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
