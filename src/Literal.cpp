#include "Literal.h"

#include <spdlog/spdlog.h>

#include <iostream>

#include "Annotation.h"
#include "Assumption.h"
#include "utility.h"

Literal::Literal(bool negated)
    : negated(negated),
      operation(PredicateOperation::constant),
      set(nullptr),
      leftLabel(std::nullopt),
      rightLabel(std::nullopt),
      identifier(std::nullopt),
      annotation(nullptr) {}

Literal::Literal(CanonicalSet set)
    : negated(false),
      operation(PredicateOperation::setNonEmptiness),
      set(set),
      leftLabel(std::nullopt),
      rightLabel(std::nullopt),
      identifier(std::nullopt),
      annotation(nullptr) {}

Literal::Literal(CanonicalSet set, CanonicalAnnotation annotation)
    : negated(true),
      operation(PredicateOperation::setNonEmptiness),
      set(set),
      leftLabel(std::nullopt),
      rightLabel(std::nullopt),
      identifier(std::nullopt),
      annotation(annotation) {}

Literal::Literal(bool negated, int leftLabel, std::string identifier)
    : negated(negated),
      operation(PredicateOperation::set),
      set(nullptr),
      leftLabel(leftLabel),
      rightLabel(std::nullopt),
      identifier(identifier),
      annotation(nullptr) {}

Literal::Literal(int leftLabel, int rightLabel, std::string identifier)
    : negated(false),
      operation(PredicateOperation::edge),
      set(nullptr),
      leftLabel(leftLabel),
      rightLabel(rightLabel),
      identifier(identifier),
      annotation(nullptr) {}

Literal::Literal(int leftLabel, int rightLabel, std::string identifier, AnnotationType value)
    : negated(true),
      operation(PredicateOperation::edge),
      set(nullptr),
      leftLabel(leftLabel),
      rightLabel(rightLabel),
      identifier(identifier),
      annotation(Annotation::newLeaf(value)) {}

Literal::Literal(bool negated, int leftLabel, int rightLabel)
    : negated(negated),
      operation(PredicateOperation::equality),
      set(nullptr),
      leftLabel(leftLabel),
      rightLabel(rightLabel),
      identifier(std::nullopt),
      annotation(nullptr) {}

bool Literal::validate() const {
  switch (operation) {
    case PredicateOperation::constant:
      return set == nullptr && !leftLabel.has_value() && !rightLabel.has_value() &&
             !identifier.has_value() && annotation == nullptr;
    case PredicateOperation::edge:
      return set == nullptr && leftLabel.has_value() && rightLabel.has_value() &&
             identifier.has_value() && (!negated || annotation != nullptr);
    case PredicateOperation::equality:
      return set == nullptr && leftLabel.has_value() && rightLabel.has_value() &&
             !identifier.has_value() && annotation == nullptr;
    case PredicateOperation::set:
      return set == nullptr && leftLabel.has_value() && !rightLabel.has_value() &&
             identifier.has_value() && annotation == nullptr;
    case PredicateOperation::setNonEmptiness:
      return set != nullptr && !leftLabel.has_value() && !rightLabel.has_value() &&
             !identifier.has_value() && (!negated || annotation != nullptr);
    default:
      return false;
  }
}

std::strong_ordering Literal::operator<=>(const Literal &other) const {
  if (const auto cmp = (other.negated <=> negated); cmp != 0) return cmp;
  if (const auto cmp = operation <=> other.operation; cmp != 0) return cmp;

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
      return std::strong_ordering::equal;  // Equal since we checked signs already
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

Literal Literal::substituteSet(AnnotatedSet set) const {
  assert(operation == PredicateOperation::setNonEmptiness);
  auto s = std::get<CanonicalSet>(set);
  auto a = std::get<CanonicalAnnotation>(set);

  if (negated) {
    return Literal(s, a);
  }
  return Literal(s);
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
    default:
      throw std::logic_error("unreachable");
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
      output += identifier->get() + "(" + std::to_string(*leftLabel) + "," +
                std::to_string(*rightLabel) + ")";
      break;
    case PredicateOperation::set:
      output += identifier->get() + "(" + std::to_string(*leftLabel) + ")";
      break;
    case PredicateOperation::equality:
      output += std::to_string(*leftLabel) + " = " + std::to_string(*rightLabel);
      break;
    case PredicateOperation::setNonEmptiness:
      output += set->toString();
      break;
    default:
      throw std::logic_error("unreachable");
  }
  return output;
}
