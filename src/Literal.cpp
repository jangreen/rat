#include "Literal.h"

#include <spdlog/spdlog.h>

#include <iostream>

Literal::Literal(bool negated, CanonicalSet set)
    : negated(negated), operation(PredicateOperation::setNonEmptiness), set(set) {}

Literal::Literal(bool negated, int leftLabel, std::string identifier)
    : negated(negated),
      operation(PredicateOperation::set),
      leftLabel(leftLabel),
      identifier(identifier) {}

Literal::Literal(bool negated, int leftLabel, int rightLabel, std::string identifier)
    : negated(negated),
      operation(PredicateOperation::edge),
      leftLabel(leftLabel),
      rightLabel(rightLabel),
      identifier(identifier) {}

Literal::Literal(bool negated, int leftLabel, int rightLabel)
    : negated(negated),
      operation(PredicateOperation::equality),
      leftLabel(leftLabel),
      rightLabel(rightLabel) {}

bool Literal::operator==(const Literal &other) const {
  return operation == other.operation && negated == other.negated && set == other.set &&
         identifier == other.identifier;
}

bool Literal::operator<(const Literal &other) const {
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
      return {};
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
        if (set->leftOperand->operation == SetOperation::singleton) {
          switch (set->rightOperand->operation) {
            case SetOperation::intersection: {
              // e & (s1 & s2) -> e & s1 , e & s2
              CanonicalSet e = set->leftOperand;
              CanonicalSet s1 = set->rightOperand->leftOperand;
              CanonicalSet s2 = set->rightOperand->rightOperand;
              CanonicalSet e_and_s1 = Set::newSet(SetOperation::intersection, e, s1);
              CanonicalSet e_and_s2 = Set::newSet(SetOperation::intersection, e, s2);

              if (negated) {
                // Rule (~&_1L)
                DNF result = {{Literal(negated, e_and_s1), Literal(negated, e_and_s1)}};
                return result;
              } else {
                // Rule (&_1L)
                DNF result = {{Literal(negated, e_and_s1)}, {Literal(negated, e_and_s1)}};
                return result;
              }
            }
            case SetOperation::image:
            case SetOperation::domain:
              // e & (sr)     or      e & (rs)
              if (set->rightOperand->leftOperand->operation == SetOperation::singleton) {
                // e1 & (e2.r)     or      e1 & (r.e2)
                if (set->rightOperand->relation->operation == RelationOperation::base) {
                  // e1 & (e2.b)     or      e1 & (b.e2)
                  // shortcut multiple rules
                  int e1 = *set->leftOperand->label;
                  int e2 = *set->rightOperand->leftOperand->label;
                  std::string b = *set->rightOperand->relation->identifier;
                  if (set->rightOperand->operation == SetOperation::image) {
                    // e1 & (e2.b)
                    DNF result = {{Literal(negated, e2, e1, b)}};
                    return result;
                  } else {
                    // e1 & (b.e2)
                    DNF result = {{Literal(negated, e1, e2, b)}};
                    return result;
                  }
                } else {
                  break;  // skip to "apply non-root rules"
                }
              } else {
                // e & sr -> re & s     or      e & rs -> er & s
                CanonicalSet e = set->leftOperand;
                CanonicalSet s = set->rightOperand->leftOperand;
                CanonicalRelation r = set->rightOperand->relation;
                if (set->rightOperand->operation == SetOperation::image) {
                  // e & sr -> re & s
                  // Rule (._12L)
                  CanonicalSet re = Set::newSet(SetOperation::domain, e, r);
                  CanonicalSet re_and_s = Set::newSet(SetOperation::intersection, re, s);
                  DNF result = {{Literal(negated, re_and_s)}};
                  return result;
                } else {
                  // e & rs -> er & s
                  // Rule (._21L)
                  CanonicalSet er = Set::newSet(SetOperation::image, e, r);
                  CanonicalSet er_and_s = Set::newSet(SetOperation::intersection, er, s);
                  DNF result = {{Literal(negated, er_and_s)}};
                  return result;
                }
              }
            default:
              break;
          }
          break;
        }
        if (set->rightOperand->operation == SetOperation::singleton) {
          switch (set->rightOperand->operation) {
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
            case SetOperation::domain:
              // (sr) & e    or      (rs) & e
              if (set->rightOperand->leftOperand->operation == SetOperation::singleton) {
                // (e2.r) & e1      or      (r.e2) & e1
                if (set->rightOperand->relation->operation == RelationOperation::base) {
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
                  break;  // skip to "apply non-root rules"
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

// std::optional<DNF> substituteHelper(bool negated, bool substituteRight,
//                                     const std::vector<std::vector<PartialPredicate>>
//                                     &disjunction, CanonicalSet otherOperand) {
//   std::optional<DNF> result = std::nullopt;
//   for (const auto &conjunction : disjunction) {
//     std::optional<Cube> cube = std::nullopt;
//     for (const auto &literal : conjunction) {
//       if (std::holds_alternative<Literal>(literal)) {
//         Literal l = std::get<Literal>(literal);
//         if (!cube) {
//           cube = Cube();
//         }
//         cube->push_back(l);
//       } else {
//         auto s = std::get<CanonicalSet>(literal);
//         // substitute
//         std::optional<Literal> l;
//         if (substituteRight) {
//           l = Literal(negated, PredicateOperation::setNonEmptiness, otherOperand, s);
//         } else {
//           l = Literal(negated, PredicateOperation::setNonEmptiness, s, otherOperand);
//         }
//         if (!cube) {
//           Cube c = {};
//           cube = c;
//         }
//         cube->push_back(*l);
//       }
//     }
//     if (!result) {
//       result = {*cube};
//     } else {
//       result->push_back(*cube);
//     }
//   }
//   return result;
// }
