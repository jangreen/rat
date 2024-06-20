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
      auto result = set->applyRule(negated, modalRules);
      if (result) {
        auto newLiterals = *result;

        DNF result;
        result.reserve(newLiterals.size());
        for (const auto &partialCube : newLiterals) {
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

// TODO: remove
// if (leftOperand->operation == SetOperation::singleton) {
//   switch (rightOperand->operation) {
//     case SetOperation::choice: {
//       Literal es1(negated, PredicateOperation::setNonEmptiness, leftOperand,
//                   rightOperand->leftOperand);
//       Literal es2(negated, PredicateOperation::setNonEmptiness, leftOperand,
//                   rightOperand->rightOperand);

//       if (negated) {
//         // Rule (\neg\cup_1\lrule): ~{e}.(s1 | s2) -> ~{e}.s1, ~{e}.s2
//         DNF result = {{es1, es2}};
//         return result;
//       } else {
//         // Rule (\cup_1\lrule): {e}.(s1 | s2) -> {e}.s1 | {e}.s2
//         DNF result = {{es1}, {es2}};
//         return result;
//       }
//     }
//     case SetOperation::intersection: {
//       Literal es1(negated, PredicateOperation::setNonEmptiness, leftOperand,
//                   rightOperand->leftOperand);
//       Literal es2(negated, PredicateOperation::setNonEmptiness, leftOperand,
//                   rightOperand->rightOperand);

//       if (negated) {
//         // Rule (\neg\cap_1\lrule): ~{e}.(s1 & s2) -> ~{e}.s1 | ~{e}.s2
//         DNF result = {{es1}, {es2}};
//         return result;
//       } else {
//         // Rule (\cap_1\lrule): {e}.(s1 & s2) -> {e}.s1, {e}.s2
//         DNF result = {{es1, es2}};
//         return result;
//       }
//     }
//     case SetOperation::empty: {
//       // Rule (bot_1): {e}.0
//       DNF result = {{BOTTOM}};
//       return result;
//     }
//     case SetOperation::full: {
//       if (negated) {
//         // Rule (\neg\top_1) + Rule (\neg=): ~{e}.T -> ~{e}.{e} -> \bot_1
//         DNF result = {{BOTTOM}};
//         return result;
//       }
//       return std::nullopt;
//     }
//     case SetOperation::singleton: {
//       // Rule (=): {e1}.{e2}
//       Literal l(negated, PredicateOperation::equality, leftOperand, rightOperand);
//       DNF result = {{l}};
//       return result;
//     }
//     case SetOperation::domain: {
//       // Rule (\comp_{1,2}): {e}.(r.s)
//       if (rightOperand->leftOperand->operation == SetOperation::singleton) {
//         // {e1}.(r.{e2})
//         if (rightOperand->relation->operation == RelationOperation::base) {
//           // fast path for {e1}.(a.e2) -> (e1,e2) \in a
//           Literal l(negated, PredicateOperation::edge, leftOperand, rightOperand->leftOperand,
//                     *rightOperand->relation->identifier);
//           DNF result = {{l}};
//           return result;
//         }
//         // 1) try reduce r.s
//         auto domainResult = rightOperand->applyRule(negated, modalRules);
//         if (domainResult) {
//           auto disjunction = *domainResult;
//           return substituteHelper(negated, true, disjunction, leftOperand);
//         }
//       } else {
//         // 2) {e}.(r.s) -> ({e}.r).s
//         CanonicalSet er = Set::newSet(SetOperation::image, leftOperand,
//         rightOperand->relation); Literal ers(negated, PredicateOperation::setNonEmptiness, er,
//         rightOperand->leftOperand); DNF result = {{ers}}; return result;
//       }
//       return std::nullopt;
//     }
//     case SetOperation::image: {
//       // {e}.(s.r)
//       if (rightOperand->leftOperand->operation == SetOperation::singleton) {
//         if (rightOperand->relation->operation == RelationOperation::base) {
//           // fast path for {e1}.(e2.a) -> (e2,e1) \in a
//           Literal l(negated, PredicateOperation::edge, rightOperand->leftOperand, leftOperand,
//                     *rightOperand->relation->identifier);
//           DNF result = {{l}};
//           return result;
//         }
//         // 1) try reduce s.r
//         auto imageResult = rightOperand->applyRule(negated, modalRules);
//         if (imageResult) {
//           auto disjunction = *imageResult;
//           return substituteHelper(negated, true, disjunction, leftOperand);
//         }
//       } else {
//         // 2) -> s.(r.{e})
//         CanonicalSet re = Set::newSet(SetOperation::domain, leftOperand,
//         rightOperand->relation); Literal sre(negated, PredicateOperation::setNonEmptiness,
//         rightOperand->leftOperand, re); DNF result = {{sre}}; return result;
//       }
//       return std::nullopt;
//     }
//     case SetOperation::base: {
//       // e.B
//       Literal l(negated, PredicateOperation::set, leftOperand, *rightOperand->identifier);
//       DNF result = {{l}};
//       return result;
//     }
//   }
// } else if (rightOperand->operation == SetOperation::singleton) {
//   switch (leftOperand->operation) {
//     case SetOperation::choice: {
//       Literal s1e(negated, PredicateOperation::setNonEmptiness, rightOperand->leftOperand,
//                   leftOperand);
//       Literal s2e(negated, PredicateOperation::setNonEmptiness, rightOperand->rightOperand,
//                   leftOperand);
//       if (negated) {
//         // Rule (\neg\cup_1\rrule): ~(s1 | s2).{e} -> ~s1{e}, ~s2{e}.
//         DNF result = {{s1e}, {s2e}};
//         return result;
//       } else {
//         // Rule (\cup_1\rrule): (s1 | s2).{e} -> s1{e}. | s2{e}.
//         DNF result = {{s1e, s2e}};
//         return result;
//       }
//     }
//     case SetOperation::intersection: {
//       Literal s1e(negated, PredicateOperation::setNonEmptiness, leftOperand->leftOperand,
//                   rightOperand);
//       Literal s2e(negated, PredicateOperation::setNonEmptiness, leftOperand->rightOperand,
//                   rightOperand);
//       if (negated) {
//         // Rule (\neg\cap_1\rrule): ~(s1 & s2).{e} -> ~s1.{e} | ~s2.{e}
//         DNF result = {{s1e}, {s2e}};
//         return result;
//       } else {
//         // Rule (\cap_1\rrule): (s1 & s2).{e} -> s1.{e}, s2.{e}
//         DNF result = {{s1e, s2e}};
//         return result;
//       }
//     }
//     case SetOperation::empty: {
//       // Rule (bot_1): 0.{e}
//       DNF result = {{BOTTOM}};
//       return result;
//     }
//     case SetOperation::full: {
//       if (negated) {
//         // Rule (\neg\top_1) + Rule (\neg=): ~T.{e} -> ~{e}.{e} -> \bot_1
//         DNF result = {{BOTTOM}};
//         return result;
//       }
//       return std::nullopt;
//     }
//     case SetOperation::singleton: {
//       // {e1}.{e2}
//       // could not happen since case is already handled above
//       std::cout << "[Error] This should not happen." << std::endl;
//       return std::nullopt;
//     }
//     case SetOperation::domain: {
//       // (r.s).{e}
//       if (leftOperand->leftOperand->operation == SetOperation::singleton) {
//         if (leftOperand->relation->operation == RelationOperation::base) {
//           // fast path for (a.e2){e1} -> (e1,e2) \in a
//           Literal l(negated, PredicateOperation::edge, rightOperand, leftOperand->leftOperand,
//                     *leftOperand->relation->identifier);
//           DNF result = {{l}};
//           return result;
//         }
//         // 1) try reduce r.s
//         auto domainResult = leftOperand->applyRule(negated, modalRules);
//         if (domainResult) {
//           auto disjunction = *domainResult;
//           return substituteHelper(negated, false, disjunction, rightOperand);
//         }
//       } else {
//         // 2) -> ({e}.r).s
//         CanonicalSet er = Set::newSet(SetOperation::image, rightOperand,
//         leftOperand->relation); Literal ers(negated, PredicateOperation::setNonEmptiness, er,
//         leftOperand->leftOperand); DNF result = {{ers}}; return result;
//       }
//       return std::nullopt;
//     }
//     case SetOperation::image: {
//       // (s.r).{e}
//       if (leftOperand->leftOperand->operation == SetOperation::singleton) {
//         if (leftOperand->relation->operation == RelationOperation::base) {
//           // fast path for (e2.a){e1} -> (e2,e1) \in a
//           Literal l(negated, PredicateOperation::edge, leftOperand->leftOperand, rightOperand,
//                     *leftOperand->relation->identifier);
//           DNF result = {{l}};
//           return result;
//         }
//         // 1) try reduce s.r
//         auto imageResult = leftOperand->applyRule(negated, modalRules);
//         if (imageResult) {
//           auto disjunction = *imageResult;
//           return substituteHelper(negated, false, disjunction, rightOperand);
//         }
//       } else {
//         // 2) -> s.(r.{e})
//         CanonicalSet re = Set::newSet(SetOperation::domain, rightOperand,
//         leftOperand->relation); Literal sre(negated, PredicateOperation::setNonEmptiness,
//         leftOperand->leftOperand, re); DNF result = {{sre}}; return result;
//       }
//       return std::nullopt;
//     }
//     case SetOperation::base: {
//       // B.e
//       Literal l(negated, PredicateOperation::set, rightOperand, *leftOperand->identifier);
//       DNF result = {{l}};
//       return result;
//     }
//   }
// } else {
//   auto leftResult = leftOperand->applyRule(negated, modalRules);
//   if (leftResult) {
//     auto disjunction = *leftResult;
//     return substituteHelper(negated, false, disjunction, rightOperand);
//   }
//   auto rightResult = rightOperand->applyRule(negated, modalRules);
//   if (rightResult) {
//     auto disjunction = *rightResult;
//     return substituteHelper(negated, true, disjunction, leftOperand);
//   }
// }
// return std::nullopt;