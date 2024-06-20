#include "Literal.h"

#include <spdlog/spdlog.h>

#include <iostream>

namespace {
    // ---------------------- Anonymous helper functions ----------------------

    std::optional<DNF> handleIntersectionWithLeftSingleton(bool negated, CanonicalSet intersection, bool modalRules) {
      // Handle "e & s != 0"
      assert(intersection->operation == SetOperation::intersection);
      CanonicalSet e = intersection->leftOperand;
      CanonicalSet s = intersection->rightOperand;
      // Do case distinction on the shape of "s"
      switch (s->operation) {
          // TODO: Handle following base cases
          case SetOperation::base:
            // e & A != 0  ->  e \in A
            assert(false);  // Not implemented
          case SetOperation::singleton:
            // e1 & e2 != 0  ->  e1 == e2
            assert(false);  // Not implemented
          case SetOperation::empty:
            // e & 0 != 0  ->  false
            assert(false);  // Not implemented
          case SetOperation::full:
            // e & 1 != 0  ->  true
            assert(false);  // Not implemented
          case SetOperation::intersection: {
            // e & (s1 & s2) -> e & s1 , e & s2
            CanonicalSet e_and_s1 = Set::newSet(SetOperation::intersection, e, s->leftOperand);
            CanonicalSet e_and_s2 = Set::newSet(SetOperation::intersection, e, s->rightOperand);

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
          // e & (s'r)     or      e & (rs')
          CanonicalSet sp = s->leftOperand;
          CanonicalRelation r = s->relation;
          if (sp->operation != SetOperation::singleton) {
            // e & s'r -> re & s'     or      e & rs' -> er & s'
            // shortcut multiple rules
            const SetOperation oppositeOp = s->operation == SetOperation::image ? SetOperation::domain : SetOperation::image;
            CanonicalSet swapped = Set::newSet(oppositeOp, e, r); // er  or  re
            CanonicalSet swapped_and_sp = Set::newSet(SetOperation::intersection, swapped, sp);
            return DNF{{Literal(negated, swapped_and_sp)}};
          } else if (r->operation == RelationOperation::base) {
            // e & (f.b)     or      e & (b.f)
            // shortcut multiple rules
            std::string b = *r->identifier;
            int first = *e->label;
            int second = *sp->label;
            if (s->operation == SetOperation::image) {
              std::swap(first, second);
            }
            // (first, second) \in b
            return DNF{{Literal(negated, first, second, b)}};
          } else {
            // Question: We know that s = fr/rf, but we don't use this fact at all here.
            // In particular, we do not refer to "f" at all anymore. Is this intended?

            // non-root rule:
            auto rightResult = s->applyRule(negated, modalRules);
            if (!rightResult) {
              // FIXME (TH): Is this allowed to happen?
              return std::nullopt;
            }

            // FIXME: The comments says the same thing twice
            // e & rightResult     or      e & rightResult
            DNF result;
            result.reserve(rightResult->size());
            for (const auto &partialCube : *rightResult) {
              Cube cube;
              cube.reserve(partialCube.size());

              for (const auto &partialLiteral : partialCube) {
                if (std::holds_alternative<Literal>(partialLiteral)) {
                  Literal l = std::get<Literal>(partialLiteral);
                  cube.push_back(std::move(l));
                } else {
                  CanonicalSet si = std::get<CanonicalSet>(partialLiteral);
                  CanonicalSet e_and_si = Set::newSet(SetOperation::intersection, e, si);
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
}

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
      assert (false);
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
          return handleIntersectionWithLeftSingleton(negated, set, modalRules);
        } else if (set->rightOperand->operation == SetOperation::singleton) {
          // TODO: Can't we just swap the order to always have (e & s) and avoid doubling the code?

          switch (set->leftOperand->operation) {
            case SetOperation::intersection: {
              // (s1 & s2) & e -> s1 & e , s2 & e
              CanonicalSet e = set->rightOperand;
              CanonicalSet s1 = set->leftOperand->leftOperand;
              CanonicalSet s2 = set->leftOperand->rightOperand;
              CanonicalSet s1_and_e = Set::newSet(SetOperation::intersection, s1, e);
              CanonicalSet s2_and_e = Set::newSet(SetOperation::intersection, s2, e);

              if (negated) {
                // Rule (~&_1R)
                DNF result = {{Literal(negated, s1_and_e), Literal(negated, s2_and_e)}};
                return result;
              } else {
                // Rule (&_1R)
                DNF result = {{Literal(negated, s1_and_e)}, {Literal(negated, s2_and_e)}};
                return result;
              }
            }
            case SetOperation::image:
            case SetOperation::domain: {
              // (sr) & e    or      (rs) & e
              if (set->leftOperand->leftOperand->operation == SetOperation::singleton) {
                // (e2.r) & e1      or      (r.e2) & e1
                if (set->leftOperand->relation->operation == RelationOperation::base) {
                  // (e2.b) & e1      or      (b.e2) & e1
                  // shortcut multiple rules
                  int e1 = *set->rightOperand->label;
                  int e2 = *set->leftOperand->leftOperand->label;
                  std::string b = *set->leftOperand->relation->identifier;
                  if (set->leftOperand->operation == SetOperation::image) {
                    // (e2.b) & e1
                    DNF result = {{Literal(negated, e2, e1, b)}};
                    return result;
                  } else {
                    // (b.e2) & e1
                    DNF result = {{Literal(negated, e1, e2, b)}};
                    return result;
                  }
                } else {
                  // non-root rule:
                  auto leftResult = set->leftOperand->applyRule(negated, modalRules);
                  if (leftResult) {
                    // leftResult & e1     or      leftResult & e1
                    DNF result;
                    result.reserve(leftResult->size());
                    for (const auto &partialCube : *leftResult) {
                      Cube cube;
                      cube.reserve(partialCube.size());

                      for (const auto &partialLiteral : partialCube) {
                        if (std::holds_alternative<Literal>(partialLiteral)) {
                          Literal l = std::get<Literal>(partialLiteral);
                          cube.push_back(std::move(l));
                        } else {
                          CanonicalSet s = std::get<CanonicalSet>(partialLiteral);
                          CanonicalSet e1 = set->rightOperand;
                          CanonicalSet s_and_e1 = Set::newSet(SetOperation::intersection, s, e1);
                          Literal l(negated, s_and_e1);
                          cube.push_back(std::move(l));
                        }
                      }
                      result.push_back(cube);
                    }
                    return result;
                  } else {
                    return std::nullopt;
                  }
                }
              } else {
                // sr & e -> s & re        or        rs & e -> s & er
                CanonicalSet e = set->rightOperand;
                CanonicalSet s = set->leftOperand->leftOperand;
                CanonicalRelation r = set->leftOperand->relation;
                if (set->rightOperand->operation == SetOperation::image) {
                  // sr & e -> s & re
                  // Rule (._12R)
                  CanonicalSet re = Set::newSet(SetOperation::domain, e, r);
                  CanonicalSet s_and_re = Set::newSet(SetOperation::intersection, s, re);
                  DNF result = {{Literal(negated, s_and_re)}};
                  return result;
                } else {
                  // rs & e -> s & er
                  // Rule (._21R)
                  CanonicalSet er = Set::newSet(SetOperation::image, e, r);
                  CanonicalSet s_and_er = Set::newSet(SetOperation::intersection, s, er);
                  DNF result = {{Literal(negated, s_and_er)}};
                  return result;
                }
              }
            }
            default:
              break;
          }
          break;
        }
      }
      // apply non-root rules
      auto result = set->applyRule(negated, modalRules);
      if (result) {
        return toDNF(negated, *result);
      }
      return std::nullopt;
  }
  assert(false); // FIXME: REACHABLE!!!
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
