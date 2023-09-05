#include "Set.h"

// RULES

template <>
std::optional<std::tuple<Set, Formula>> Set::applyRule<ProofRule::a>() {
  if (operation == SetOperation::domain && leftOperand->operation == SetOperation::singleton &&
      relation->operation == RelationOperation::base) {
    // replace (a.e) with (f)
    Set freshEvent(SetOperation::singleton, Set::maxSingletonLabel++);
    Set aE(*this);

    Predicate p(PredicateOperation::intersectionNonEmptiness, std::move(freshEvent),
                std::move(Set(aE)));
    Literal l(false, p);
    Formula f(FormulaOperation::literal, std::move(l));

    // construct return value
    std::tuple<Set, Formula> result{std::move(aE), std::move(f)};
    return result;
  } else if (operation == SetOperation::image &&
             leftOperand->operation == SetOperation::singleton &&
             relation->operation == RelationOperation::base) {
    // replace (e.a) with (f)
    Set freshEvent(SetOperation::singleton, Set::maxSingletonLabel++);
    Set aE(*this);

    Predicate p(PredicateOperation::intersectionNonEmptiness, std::move(freshEvent),
                std::move(Set(aE)));
    Literal l(false, p);
    Formula f(FormulaOperation::literal, std::move(l));

    // construct return value
    std::tuple<Set, Formula> result{std::move(aE), std::move(f)};
    return result;
  }
  return std::nullopt;
}

std::string Set::toString() const {
  std::string output;
  switch (operation) {
    case SetOperation::singleton:
      output += "[" + std::to_string(*label) + "]";
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
      output += "E";
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