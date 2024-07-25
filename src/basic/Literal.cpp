#include "Literal.h"

#include <spdlog/spdlog.h>

#include <iostream>

#include "../Assumption.h"
#include "../utility.h"
#include "Annotation.h"

Literal::Literal(bool negated)
    : negated(negated),
      operation(PredicateOperation::constant),
      set(nullptr),
      annotation(Annotation::none()),
      leftEvent(nullptr),
      rightEvent(nullptr),
      identifier(std::nullopt) {}

Literal::Literal(const CanonicalSet set)
    : negated(false),
      operation(PredicateOperation::setNonEmptiness),
      set(set),
      annotation(Annotation::none()),
      leftEvent(nullptr),
      rightEvent(nullptr),
      identifier(std::nullopt) {}

Literal::Literal(const AnnotatedSet &annotatedSet)
    : negated(true),
      operation(PredicateOperation::setNonEmptiness),
      set(std::get<CanonicalSet>(annotatedSet)),
      annotation(std::get<CanonicalAnnotation>(annotatedSet)),
      leftEvent(nullptr),
      rightEvent(nullptr),
      identifier(std::nullopt) {
  assert(Annotated::validate(annotatedSet));
}

Literal::Literal(bool negated, CanonicalSet event, std::string identifier)
    : negated(negated),
      operation(PredicateOperation::set),
      set(nullptr),
      annotation(Annotation::none()),
      leftEvent(event),
      rightEvent(nullptr),
      identifier(identifier) {}

Literal::Literal(CanonicalSet leftEvent, CanonicalSet rightEvent, std::string identifier)
    : negated(false),
      operation(PredicateOperation::edge),
      set(nullptr),
      annotation(Annotation::none()),
      leftEvent(leftEvent),
      rightEvent(rightEvent),
      identifier(identifier) {
  assert(leftEvent->isEvent());
  assert(rightEvent->isEvent());
}

Literal::Literal(CanonicalSet leftEvent, CanonicalSet rightEvent, std::string identifier,
                 const AnnotationType &annotation)
    : negated(true),
      operation(PredicateOperation::edge),
      set(nullptr),
      annotation(Annotation::newLeaf(annotation)),
      leftEvent(leftEvent),
      rightEvent(rightEvent),
      identifier(identifier) {
  assert(leftEvent->isEvent());
  assert(rightEvent->isEvent());
}

Literal::Literal(bool negated, CanonicalSet leftEvent, CanonicalSet rightEvent)
    : negated(negated),
      operation(PredicateOperation::equality),
      set(nullptr),
      annotation(Annotation::none()),
      leftEvent(leftEvent),
      rightEvent(rightEvent),
      identifier(std::nullopt) {}

bool Literal::validate() const {
  switch (operation) {
    case PredicateOperation::constant:
      return set == nullptr && leftEvent == nullptr && rightEvent == nullptr &&
             !identifier.has_value() && annotation == Annotation::none();
    case PredicateOperation::edge:
      return set == nullptr && leftEvent != nullptr && rightEvent != nullptr &&
             leftEvent->isEvent() && rightEvent->isEvent() && identifier.has_value() &&
             (!negated || annotation != Annotation::none());
    case PredicateOperation::equality:
      return set == nullptr && leftEvent != nullptr && rightEvent != nullptr &&
             leftEvent->isEvent() && rightEvent->isEvent() && !identifier.has_value() &&
             annotation == Annotation::none();
    case PredicateOperation::set:
      return set == nullptr && leftEvent != nullptr && rightEvent == nullptr &&
             leftEvent->isEvent() && identifier.has_value() && annotation == Annotation::none();
    case PredicateOperation::setNonEmptiness:
      return set != nullptr && leftEvent == nullptr && rightEvent == nullptr &&
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
      auto cmp = leftEvent <=> other.leftEvent;
      cmp = (cmp != 0) ? cmp : (rightEvent <=> other.rightEvent);
      cmp = (cmp != 0) ? cmp : lexCompare(*identifier, *other.identifier);
      return cmp;
    }
    case PredicateOperation::set: {
      auto cmp = leftEvent <=> other.leftEvent;
      cmp = (cmp != 0) ? cmp : lexCompare(*identifier, *other.identifier);
      return cmp;
    }
    case PredicateOperation::equality: {
      auto cmp = leftEvent <=> other.leftEvent;
      cmp = (cmp != 0) ? cmp : (rightEvent <=> other.rightEvent);
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
         leftEvent == other.leftEvent && rightEvent == other.rightEvent &&
         identifier == other.identifier;
}

// a literal is normal if it cannot be simplified
bool Literal::isNormal() const {
  switch (operation) {
    case PredicateOperation::setNonEmptiness: {
      return set->isNormal();
    }
    case PredicateOperation::constant:
      return false;
    case PredicateOperation::equality:
      return negated && rightEvent != leftEvent;
    case PredicateOperation::set:
    case PredicateOperation::edge:
      return true;
    default:
      throw std::logic_error("unreachable");
  }
}

bool Literal::isPositiveEdgePredicate() const {
  return !negated && operation == PredicateOperation::edge;
}

bool Literal::isPositiveEqualityPredicate() const {
  return !negated && operation == PredicateOperation::equality;
}

EventSet Literal::normalEvents() const {
  switch (operation) {
    case PredicateOperation::constant:
      return {};
    case PredicateOperation::setNonEmptiness: {
      return set->getNormalEvents();
    }
    case PredicateOperation::edge:
    case PredicateOperation::equality: {
      if (!isNormal()) {
        return {};
      }
      auto events = leftEvent->getEvents();
      auto rightEvents = rightEvent->getEvents();
      events.insert(rightEvents.begin(), rightEvents.end());
      return events;
    }
    case PredicateOperation::set: {
      return leftEvent->getEvents();
    }
    default:
      throw std::logic_error("unreachable");
  }
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
      auto events = leftEvent->getEvents();
      auto rightEvents = rightEvent->getEvents();
      events.insert(rightEvents.begin(), rightEvents.end());
      return events;
    }
    case PredicateOperation::set: {
      return leftEvent->getEvents();
    }
    default:
      throw std::logic_error("unreachable");
  }
}

EventSet Literal::topEvents() const {
  switch (operation) {
    case PredicateOperation::setNonEmptiness: {
      return set->getTopEvents();
    }
    case PredicateOperation::constant:
      return {};
    case PredicateOperation::equality:
    case PredicateOperation::edge: {
      auto events = leftEvent->getTopEvents();
      auto rightEvents = rightEvent->getTopEvents();
      events.insert(rightEvents.begin(), rightEvents.end());
      return events;
    }
    case PredicateOperation::set:
      return leftEvent->getTopEvents();
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
      const CanonicalSet e1 = leftEvent;
      const CanonicalSet e2 = rightEvent;
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
  switch (operation) {
    case PredicateOperation::setNonEmptiness: {
      const auto newSet = Annotated::substituteAll(annotatedSet(), search, replace);
      if (newSet.first != set) {
        return Literal(newSet);
      }
      return std::nullopt;
    }
    case PredicateOperation::constant:
      return std::nullopt;
    case PredicateOperation::equality:
    case PredicateOperation::edge: {
      assert(annotation->isLeaf() && annotation->getValue().has_value());
      if (!search->isEvent() || !replace->isEvent()) {
        return std::nullopt;
      }
      const auto left = leftEvent == search ? replace : leftEvent;
      const auto right = rightEvent == search ? replace : rightEvent;

      if (left != leftEvent || right != rightEvent) {
        return negated ? Literal(left, right, identifier.value(), annotation->getValue().value())
                       : Literal(left, right, identifier.value());
      }
      return std::nullopt;
    }
    case PredicateOperation::set: {
      if (!search->isEvent() || !replace->isEvent()) {
        return std::nullopt;
      }
      const auto left = leftEvent == search ? replace : leftEvent;

      if (left != leftEvent) {
        return Literal(negated, left, identifier.value());
      }
      return std::nullopt;
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
      leftEvent = leftEvent->rename(renaming);
      rightEvent = rightEvent->rename(renaming);
      return;
    }
    case PredicateOperation::set: {
      leftEvent = leftEvent->rename(renaming);
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
      output +=
          identifier->get() + "(" + leftEvent->toString() + "," + rightEvent->toString() + ")";
      break;
    case PredicateOperation::set:
      output += identifier->get() + "(" + leftEvent->toString() + ")";
      break;
    case PredicateOperation::equality:
      output += leftEvent->toString() + " = " + rightEvent->toString();
      break;
    case PredicateOperation::setNonEmptiness:
      output += set->toString();
      break;
    default:
      throw std::logic_error("unreachable");
  }
  return output;
}
