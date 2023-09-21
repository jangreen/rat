#include "Predicate.h"
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
Set::Set(const std::string &expression) {
  Logic visitor;
  // TODO: *this = visitor.parseSet(expression);
}
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
  auto isEqual = operation == other.operation;
  if ((leftOperand == nullptr) != (other.leftOperand == nullptr)) {
    isEqual = false;
  } else if (leftOperand != nullptr && *leftOperand != *other.leftOperand) {
    isEqual = false;
  } else if ((rightOperand == nullptr) != (other.rightOperand == nullptr)) {
    isEqual = false;
  } else if (rightOperand != nullptr && *rightOperand != *other.rightOperand) {
    isEqual = false;
  } else if ((relation == nullptr) != (other.relation == nullptr)) {
    isEqual = false;
  } else if (relation != nullptr && *relation != *other.relation) {
    isEqual = false;
  } else if (label.has_value() != other.label.has_value()) {
    isEqual = false;
  } else if (label.has_value() && *label != *other.label) {
    isEqual = false;
  } else if (identifier.has_value() != other.identifier.has_value()) {
    isEqual = false;
  } else if (identifier.has_value() && *identifier != *other.identifier) {
    isEqual = false;
  }
  return isEqual;
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

std::optional<std::vector<std::vector<Set::PartialPredicate>>> Set::applyRule(bool modalRules) {
  std::vector<std::vector<PartialPredicate>> result;
  switch (operation) {
    case SetOperation::singleton:
      return std::nullopt;
    case SetOperation::empty:  // TODO: handle in choice/intersection/domain/image
      return std::nullopt;
    case SetOperation::full:  // TODO: handle in choice/intersection/domain/image
      return std::nullopt;
    case SetOperation::choice:
      // [A | B] -> { [A] },{ [B] }
      result = {{Set(*leftOperand)}, {Set(*rightOperand)}};
      return result;
    case SetOperation::intersection:
      if (leftOperand->operation == SetOperation::singleton) {
        // [e & S] -> { [e], e.S }
        Predicate p(PredicateOperation::intersectionNonEmptiness, Set(*leftOperand),
                    Set(*rightOperand));
        result = {{Set(*leftOperand), std::move(p)}};
        return result;
      } else if (rightOperand->operation == SetOperation::singleton) {
        // [S & e] -> { [e], e.S }
        Predicate p(PredicateOperation::intersectionNonEmptiness, Set(*rightOperand),
                    Set(*leftOperand));
        result = {{Set(*rightOperand), std::move(p)}};
        return result;
      } else {
        // [S1 & S2]
      }

      return std::nullopt;
    case SetOperation::base: {
      // [B] -> { [f], B.f }
      Set f(SetOperation::singleton, Set::maxSingletonLabel++);
      Predicate p(PredicateOperation::intersectionNonEmptiness, Set(*this), Set(f));
      result = {{std::move(f), p}};
      return result;
    }
    case SetOperation::image:
      if (leftOperand->operation == SetOperation::singleton) {
        switch (relation->operation) {
          case RelationOperation::base: {
            // TODO: implement (only use if want to)
            if (!modalRules) {
              return std::nullopt;
            }
            // [e.b] -> { [f], (e.b)f }
            Set f(SetOperation::singleton, Set::maxSingletonLabel++);
            Predicate p(PredicateOperation::intersectionNonEmptiness, Set(*this), Set(f));
            // result = {{std::move(f), p}};
            // currently replace
            swap(*this, f);
            result = {{p}};
            return result;
          }
          case RelationOperation::cartesianProduct:
            return std::nullopt;  // TODO: implement
          case RelationOperation::choice: {
            // [e.(r1 | r2)] -> { [e.r1] }, { [e.r2] }
            Set er1(SetOperation::image, Set(*leftOperand), Relation(*relation->leftOperand));
            Set er2(SetOperation::image, Set(*leftOperand), Relation(*relation->rightOperand));
            result = {{std::move(er1)}, {std::move(er2)}};
            return result;
          }
          case RelationOperation::composition: {
            // [e(a.b)] -> { [(e.a)b] }
            Set ea(SetOperation::image, Set(*leftOperand), Relation(*relation->leftOperand));
            Set ea_b(SetOperation::image, std::move(ea), Relation(*relation->rightOperand));
            result = {{std::move(ea_b)}};
            return result;
          }
          case RelationOperation::converse: {
            // [e.(r^-1)] -> { [r.e] }
            Set re(SetOperation::domain, Set(*leftOperand), Relation(*relation->leftOperand));
            result = {{std::move(re)}};
            return result;
          }
          case RelationOperation::empty:
            // [e.0] -> { [0] }
            // TODO: implement
            return std::nullopt;
          case RelationOperation::full:
            // [e.T] -> { [T] }
            // TODO: implement
            return std::nullopt;
          case RelationOperation::identity: {
            // [e.id] -> { [e] }
            Set e(SetOperation::singleton, Set(*leftOperand));
            result = {{std::move(e)}};
            return result;
          }
          case RelationOperation::intersection: {
            // [e.(r1 & r2)] -> { [e.r1 & e.r2] }
            Set er1(SetOperation::image, Set(*leftOperand), Relation(*relation->leftOperand));
            Set er2(SetOperation::image, Set(*leftOperand), Relation(*relation->rightOperand));
            Set er1_and_er2(SetOperation::intersection, std::move(er1), std::move(er2));
            result = {{std::move(er1_and_er2)}};
            return result;
          }
          case RelationOperation::transitiveClosure: {
            // [e.r*] -> { [(e.r)r*] }, { [e] }
            Set er(SetOperation::image, Set(*leftOperand), Relation(*relation->leftOperand));
            Set err_star(SetOperation::image, std::move(er), Relation(*relation));
            Set e(*leftOperand);

            result = {{std::move(err_star)}, {std::move(e)}};
            return result;
          }
        }
      } else {
        auto leftOperandResultOptional = leftOperand->applyRule(modalRules);
        if (leftOperandResultOptional) {
          auto leftOperandResult = *leftOperandResultOptional;
          for (const auto &clause : leftOperandResult) {
            std::vector<PartialPredicate> newClause;
            for (const auto &partialPredicate : clause) {
              if (std::holds_alternative<Predicate>(partialPredicate)) {
                newClause.push_back(std::move(partialPredicate));
              } else {
                auto set = std::get<Set>(partialPredicate);
                // TODO: there should only be one [] inside eache {}
                // otherwise we have to instersect (&) all  []'s after before replacing
                // currently we jsut assume this is the case without further checking
                Set newSet(SetOperation::image, std::move(set), Relation(*relation));
                newClause.push_back(std::move(newSet));
              }
            }
            result.push_back(std::move(newClause));
          }
          return result;
        }
        return std::nullopt;
      }
    case SetOperation::domain:
      if (leftOperand->operation == SetOperation::singleton) {
        switch (relation->operation) {
          case RelationOperation::base: {
            // TODO: implement (only use if want to)
            if (!modalRules) {
              return std::nullopt;
            }

            // [b.e] -> { [f], (b.e)f }
            Set f(SetOperation::singleton, Set::maxSingletonLabel++);
            Predicate p(PredicateOperation::intersectionNonEmptiness, Set(*this), Set(f));
            // result = {{std::move(f), p}};
            // currently replace
            swap(*this, f);
            result = {{p}};
            return result;
          }
          case RelationOperation::cartesianProduct:
            return std::nullopt;  // TODO: implement
          case RelationOperation::choice: {
            // [(r1 | r2).e] -> { [r1.e] }, { [r2.e] }
            Set r1e(SetOperation::domain, Set(*leftOperand), Relation(*relation->leftOperand));
            Set r2e(SetOperation::domain, Set(*leftOperand), Relation(*relation->rightOperand));
            result = {{std::move(r1e)}, {std::move(r2e)}};
            return result;
          }
          case RelationOperation::composition: {
            // [(a.b)e] -> { [a(b.e)] }
            Set be(SetOperation::domain, Set(*leftOperand), Relation(*relation->rightOperand));
            Set a_be(SetOperation::domain, std::move(be), Relation(*relation->leftOperand));
            result = {{std::move(a_be)}};
            return result;
          }
          case RelationOperation::converse: {
            // [(r^-1)e] -> { [e.r] }
            Set er(SetOperation::image, Set(*leftOperand), Relation(*relation->leftOperand));
            result = {{std::move(er)}};
            return result;
          }
          case RelationOperation::empty:
            // [e.0] -> { [0] }
            // TODO: implement
            return std::nullopt;
          case RelationOperation::full:
            // [e.T] -> { [T] }
            // TODO: implement
            return std::nullopt;
          case RelationOperation::identity: {
            // [id.e] -> { [e] }
            Set e(SetOperation::singleton, Set(*rightOperand));
            result = {{std::move(e)}};
            return result;
          }
          case RelationOperation::intersection: {
            // [(r1 & r2).e] -> { [r1.e & r2.e] }
            Set r1e(SetOperation::domain, Set(*leftOperand), Relation(*relation->leftOperand));
            Set r2e(SetOperation::domain, Set(*leftOperand), Relation(*relation->rightOperand));
            Set r1e_and_r2e(SetOperation::intersection, std::move(r1e), std::move(r2e));
            result = {{std::move(r1e_and_r2e)}};
            return result;
          }
          case RelationOperation::transitiveClosure: {
            // [r*.e] -> { [r*.(r.e)] }, { [e] }
            Set re(SetOperation::domain, Set(*leftOperand), Relation(*relation->leftOperand));
            Set r_star_re(SetOperation::domain, std::move(re), Relation(*relation));
            Set e(*leftOperand);

            result = {{std::move(r_star_re)}, {std::move(e)}};
            return result;
          }
        }
      } else {
        auto leftOperandResultOptional = leftOperand->applyRule(modalRules);
        if (leftOperandResultOptional) {
          auto leftOperandResult = *leftOperandResultOptional;
          for (const auto &clause : leftOperandResult) {
            std::vector<PartialPredicate> newClause;
            for (const auto &partialPredicate : clause) {
              if (std::holds_alternative<Predicate>(partialPredicate)) {
                newClause.push_back(std::move(partialPredicate));
              } else {
                auto set = std::get<Set>(partialPredicate);
                // TODO: there should only be one [] inside eache {}
                // otherwise we have to instersect (&) all  []'s after before replacing
                // currently we jsut assume this is the case without further checking
                Set newSet(SetOperation::domain, std::move(set), Relation(*relation));
                newClause.push_back(std::move(newSet));
              }
            }
            result.push_back(std::move(newClause));
          }
          return result;
        }
        return std::nullopt;
      }
  }
}

bool Set::substitute(const Set &search, const Set &replace) {
  if (*this == search) {
    *this = replace;
    return true;
  } else {
    switch (operation) {
      case SetOperation::choice:
        return leftOperand->substitute(search, replace) || leftOperand->substitute(search, replace);
      case SetOperation::intersection:
        return leftOperand->substitute(search, replace) || leftOperand->substitute(search, replace);
      case SetOperation::domain:
        return leftOperand->substitute(search, replace);
      case SetOperation::image:
        return leftOperand->substitute(search, replace);
      default:
        return false;
    }
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

// RULES
/* LEGACY
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
*/

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

/* LEGACY
std::vector<int> Relation::labels() const {
  if (label) {
    return {*label};
  } else if (leftOperand == nullptr && rightOperand == nullptr) {
    return {};
  }

  auto result = leftOperand->labels();
  if (rightOperand != nullptr) {
    auto right = rightOperand->labels();
    // only unique
    for (int i : right) {
      if (std::find(result.begin(), result.end(), i) == result.end()) {
        result.push_back(i);
      }
    }
  }
  return result;
}

std::vector<int> Relation::calculateRenaming() const {
  return labels();  // labels already calculates the renaming
}

void Relation::rename(const std::vector<int> &renaming) {
  if (label) {
    label = distance(renaming.begin(), find(renaming.begin(), renaming.end(), *label));
  } else if (leftOperand) {
    leftOperand->rename(renaming);
    if (rightOperand) {
      rightOperand->rename(renaming);
    }
  }
}

void Relation::inverseRename(const std::vector<int> &renaming) {
  if (label) {
    label = renaming[*label];
  } else if (leftOperand) {
    leftOperand->inverseRename(renaming);
    if (rightOperand) {
      rightOperand->inverseRename(renaming);
    }
  }
}
*/