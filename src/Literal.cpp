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
      annotation(Annotation::none()),
      leftLabel(std::nullopt),
      rightLabel(std::nullopt),
      identifier(std::nullopt) {}

Literal::Literal(const CanonicalSet set)
    : negated(false),
      operation(PredicateOperation::setNonEmptiness),
      set(set),
      annotation(Annotation::none()),
      leftLabel(std::nullopt),
      rightLabel(std::nullopt),
      identifier(std::nullopt) {}

Literal::Literal(const AnnotatedSet &annotatedSet)
    : negated(true),
      operation(PredicateOperation::setNonEmptiness),
      set(std::get<CanonicalSet>(annotatedSet)),
      annotation(std::get<CanonicalAnnotation>(annotatedSet)),
      leftLabel(std::nullopt),
      rightLabel(std::nullopt),
      identifier(std::nullopt) {
  assert(Annotated::validate(annotatedSet));
}

Literal::Literal(bool negated, int leftLabel, std::string identifier)
    : negated(negated),
      operation(PredicateOperation::set),
      set(nullptr),
      annotation(Annotation::none()),
      leftLabel(leftLabel),
      rightLabel(std::nullopt),
      identifier(identifier) {}

Literal::Literal(int leftLabel, int rightLabel, std::string identifier)
    : negated(false),
      operation(PredicateOperation::edge),
      set(nullptr),
      annotation(Annotation::none()),
      leftLabel(leftLabel),
      rightLabel(rightLabel),
      identifier(identifier) {}

Literal::Literal(int leftLabel, int rightLabel, std::string identifier, const AnnotationType &annotation)
    : negated(true),
      operation(PredicateOperation::edge),
      set(nullptr),
      annotation(Annotation::newLeaf(annotation)),
      leftLabel(leftLabel),
      rightLabel(rightLabel),
      identifier(identifier) {}

Literal::Literal(bool negated, int leftLabel, int rightLabel)
    : negated(negated),
      operation(PredicateOperation::equality),
      set(nullptr),
      annotation(Annotation::none()),
      leftLabel(leftLabel),
      rightLabel(rightLabel),
      identifier(std::nullopt) {}

bool Literal::validate() const {
  switch (operation) {
    case PredicateOperation::constant:
      return set == nullptr && !leftLabel.has_value() && !rightLabel.has_value() &&
             !identifier.has_value() && annotation == Annotation::none();
    case PredicateOperation::edge:
      return set == nullptr && leftLabel.has_value() && rightLabel.has_value() &&
             identifier.has_value() && (!negated || annotation != Annotation::none());
    case PredicateOperation::equality:
      return set == nullptr && leftLabel.has_value() && rightLabel.has_value() &&
             !identifier.has_value() && annotation == Annotation::none();
    case PredicateOperation::set:
      // check annotations for negated literals
      assert(!negated || Annotated::validate(annotatedSet()));

      return set == nullptr && leftLabel.has_value() && !rightLabel.has_value() &&
             identifier.has_value() && annotation == Annotation::none();
    case PredicateOperation::setNonEmptiness:
      return set != nullptr && !leftLabel.has_value() && !rightLabel.has_value() &&
             !identifier.has_value() /*&& (!negated || annotation != Annotation::none()) */;
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
  // TODO: Compare annotation?
  return operation == other.operation && negated != other.negated && set == other.set &&
         leftLabel == other.leftLabel && rightLabel == other.rightLabel &&
         identifier == other.identifier;
}

// a literal is normal if it cannot be simplified
bool Literal::isNormal() const {
  switch (operation) {
    case PredicateOperation::setNonEmptiness: {
      if (set->operation == SetOperation::setIntersection &&
          (set->leftOperand->isEvent() || set->rightOperand->isEvent())) {
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

bool Literal::hasTopEvent() const {
  if (operation == PredicateOperation::setNonEmptiness) {
    return set->hasTopEvent();
  }
  return false;
}

bool Literal::isPositiveEdgePredicate() const {
  return !negated && operation == PredicateOperation::edge;
}

bool Literal::isPositiveEqualityPredicate() const {
  return !negated && operation == PredicateOperation::equality;
}

EventSet Literal::events() const {
  switch (operation) {
    case PredicateOperation::constant:
      return {};
    case PredicateOperation::setNonEmptiness: {
      return set->getEvents();
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

EventSet Literal::topEvents() const {
  switch (operation) {
    case PredicateOperation::constant:
    case PredicateOperation::edge:
    case PredicateOperation::equality:
    case PredicateOperation::set:
      return {};
    case PredicateOperation::setNonEmptiness: {
      return set->getTopEvents();
    }
    default:
      throw std::logic_error("unreachable");
  }
}

SetOfSets Literal::labelBaseCombinations() const {
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

std::optional<Literal> Literal::substituteAll(const CanonicalSet search,
                                              const CanonicalSet replace) const {
  if (operation != PredicateOperation::setNonEmptiness) {
    // only substitute in set expressions
    return std::nullopt;
  }

  const auto newSet = Annotated::substituteAll(annotatedSet(), search, replace);
  if (newSet.first != set) {
    return Literal(newSet);
  }
  return std::nullopt;
}

bool Literal::substitute(const CanonicalSet search, const CanonicalSet replace, int n) {
  if (operation != PredicateOperation::setNonEmptiness) {
    // only substitute in set expressions
    return false;
  }

  const auto [subSet, subAnnotation] = Annotated::substitute(annotatedSet(), search, replace, &n);
  if (subSet != set) {
    set = subSet;
    annotation = subAnnotation;
    return true;
  }
  return false;
}

Literal Literal::substituteSet(const AnnotatedSet &set) const {
  assert(operation == PredicateOperation::setNonEmptiness);
  if (negated) {
    return Literal(set);
  }
  return Literal(std::get<CanonicalSet>(set));
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
