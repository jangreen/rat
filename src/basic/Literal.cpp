#include "Literal.h"

#include <spdlog/spdlog.h>

#include <iostream>

#include "../Assumption.h"
#include "../tableau/Rules.h"
#include "../utility.h"
#include "annotations/LeafAnnotation.h"

Literal::Literal(const bool negated, const PredicateOperation operation, const CanonicalSet set,
                 const CanonicalLeafAnnotation<Reasons> annotation, const CanonicalSet leftEvent,
                 const CanonicalSet rightEvent, std::optional<CanonicalString> identifier)
    : negated(negated),
      operation(operation),
      set(set),
      annotation(annotation),
      leftEvent(leftEvent),
      rightEvent(rightEvent),
      identifier(std::move(identifier)) {}

Literal Literal::newSetNonEmptiness(const bool negated, const CanonicalSet set,
                                    const CanonicalLeafAnnotation<Reasons> annotation) {
  return Literal(negated, PredicateOperation::setNonEmptiness, set, annotation, nullptr, nullptr,
                 std::nullopt);
}

Literal Literal::newSetMembership(bool negated, const CanonicalSet event,
                                  CanonicalString identifier,
                                  const CanonicalLeafAnnotation<Reasons> annotation) {
  return Literal(negated, PredicateOperation::set, nullptr, annotation, event, nullptr, identifier);
}

Literal Literal::newRelationMembership(bool negated, const CanonicalSet leftEvent,
                                       const CanonicalSet rightEvent, CanonicalString identifier,
                                       const CanonicalLeafAnnotation<Reasons> annotation) {
  assert(leftEvent->isEvent());
  assert(rightEvent->isEvent());
  return Literal(negated, PredicateOperation::edge, nullptr, annotation, leftEvent, rightEvent,
                 identifier);
}
Literal Literal::newOutgoingEdge(bool negated, CanonicalSet leftEvent, CanonicalString identifier,
                                 CanonicalLeafAnnotation<Reasons> annotation) {
  return Literal(negated, PredicateOperation::outgoingEdge, nullptr, annotation, leftEvent, nullptr,
                 identifier);
}
Literal Literal::newIncomingEdge(bool negated, CanonicalSet leftEvent, CanonicalString identifier,
                                 CanonicalLeafAnnotation<Reasons> annotation) {
  return Literal(negated, PredicateOperation::incomingEdge, nullptr, annotation, leftEvent, nullptr,
                 identifier);
}

Literal Literal::newEquality(bool negated, CanonicalSet leftEvent, CanonicalSet rightEvent,
                             const CanonicalLeafAnnotation<Reasons> annotation) {
  return Literal(negated, PredicateOperation::equality, nullptr, annotation, leftEvent, rightEvent,
                 std::nullopt);
}

Literal Literal::BOTTOM() {
  static const auto BOTTOM =
      Literal(true, PredicateOperation::constant, nullptr, LeafAnnotation<Reasons>::newLeaf({}),
              nullptr, nullptr, std::nullopt);
  return BOTTOM;
}

Literal Literal::TOP() {
  static const auto TOP =
      Literal(false, PredicateOperation::constant, nullptr, LeafAnnotation<Reasons>::newLeaf({}),
              nullptr, nullptr, std::nullopt);
  return TOP;
}

bool Literal::validate() const {
  switch (operation) {
    case PredicateOperation::constant: {
      const auto isValid = set == nullptr && leftEvent == nullptr && rightEvent == nullptr &&
                           !identifier.has_value() &&
                           annotation == LeafAnnotation<Reasons>::newLeaf({});
      assert(isValid);
      return isValid;
    }
    case PredicateOperation::edge: {
      const auto isValid = set == nullptr && leftEvent != nullptr && rightEvent != nullptr &&
                           leftEvent->isEvent() && rightEvent->isEvent() &&
                           identifier.has_value() && (!negated || annotation->hasValue());
      assert(isValid);
      return isValid;
    }
    case PredicateOperation::incomingEdge:
    case PredicateOperation::outgoingEdge: {
      const auto isValid = set == nullptr && leftEvent != nullptr && rightEvent == nullptr &&
                           leftEvent->isEvent() && identifier.has_value() &&
                           (!negated || annotation->hasValue());
      assert(isValid);
      return isValid;
    }
    case PredicateOperation::equality: {
      const auto isValid = set == nullptr && leftEvent != nullptr && rightEvent != nullptr &&
                           leftEvent->isEvent() && rightEvent->isEvent() && !identifier.has_value();
      assert(isValid);
      return isValid;
    }
    case PredicateOperation::set: {
      const auto isValid = set == nullptr && leftEvent != nullptr && rightEvent == nullptr &&
                           leftEvent->isEvent() && identifier.has_value() &&
                           (!negated || annotation->hasValue());
      assert(isValid);
      return isValid;
    }
    case PredicateOperation::setNonEmptiness: {
      const auto isValid = set != nullptr && leftEvent == nullptr && rightEvent == nullptr &&
                           !identifier.has_value() /*&& (!negated || annotation.hasValue()) */;
      assert(isValid);
      assert(!negated || validateReasons(annotatedSet()));
      return isValid;
    }
    default:
      return false;
  }
}

std::strong_ordering Literal::operator<=>(const Literal &other) const {
  if (const auto cmp = (other.negated <=> negated); cmp != 0) {
    return cmp;
  }
  if (const auto cmp = operation <=> other.operation; cmp != 0) {
    return cmp;
  }

  // We assume well-formed literals
  switch (operation) {
    case PredicateOperation::edge: {
      auto cmp = leftEvent <=> other.leftEvent;
      cmp = (cmp != 0) ? cmp : (rightEvent <=> other.rightEvent);
      cmp = (cmp != 0) ? cmp : lexCompare(*identifier, *other.identifier);
      return cmp;
    }
    case PredicateOperation::incomingEdge:
    case PredicateOperation::outgoingEdge: {
      auto cmp = leftEvent <=> other.leftEvent;
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
      return negated && rightEvent != leftEvent || !negated;
    case PredicateOperation::set:
    case PredicateOperation::edge:
    case PredicateOperation::incomingEdge:
    case PredicateOperation::outgoingEdge:
      return true;
    default:
      throw std::logic_error("unreachable");
  }
}
bool Literal::hasFullSet() const {
  return operation == PredicateOperation::setNonEmptiness && set->hasFullSet();
}
bool Literal::hasBaseSet() const {
  return operation == PredicateOperation::setNonEmptiness && set->hasBaseSet();
}
bool Literal::isPositiveEdgePredicate() const {
  return !negated && operation == PredicateOperation::edge;
}
bool Literal::isPositiveSetPredicate() const {
  return !negated && operation == PredicateOperation::set;
}
bool Literal::isNegatedAtomic() const {
  return negated && operation != PredicateOperation::setNonEmptiness;
}
bool Literal::isPositiveAtomic() const {
  return !negated && operation != PredicateOperation::setNonEmptiness;
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
    case PredicateOperation::incomingEdge:
    case PredicateOperation::outgoingEdge: {
      if (!isNormal()) {
        return {};
      }
      return leftEvent->getEvents();
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
    case PredicateOperation::incomingEdge:
    case PredicateOperation::outgoingEdge:
    case PredicateOperation::set: {
      return leftEvent->getEvents();
    }
    default:
      throw std::logic_error("unreachable");
  }
}

SetOfSets Literal::baseSets() const {
  switch (operation) {
    case PredicateOperation::constant:
    case PredicateOperation::equality:
    case PredicateOperation::set:
      return {Set::newBaseSet(set->identifier.value())};
    case PredicateOperation::setNonEmptiness: {
      return set->getBaseSets();
    }
    case PredicateOperation::edge:
    case PredicateOperation::incomingEdge:
    case PredicateOperation::outgoingEdge:
      return {};
    default:
      throw std::logic_error("unreachable");
  }
}

// event base pair is an event together with a base relation/base set
SetOfSets Literal::eventBasePairs() const {
  switch (operation) {
    case PredicateOperation::constant:
    case PredicateOperation::equality:
    case PredicateOperation::set:
      return {};
    case PredicateOperation::setNonEmptiness: {
      return set->getEventBasePairs();
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
    case PredicateOperation::incomingEdge:
    case PredicateOperation::outgoingEdge: {
      const CanonicalSet e1 = leftEvent;
      const CanonicalRelation b = Relation::newBaseRelation(*identifier);
      const CanonicalSet e1b = Set::newSet(SetOperation::image, e1, b);
      return {e1b};
    }
    default:
      throw std::logic_error("unreachable");
  }
}

SetOfSets getSaturatedEventBasePairs(const LeafAnnotatedSet<Reasons> &annotatedSet) {
  const auto &[set, annotation] = annotatedSet;

  switch (set->operation) {
    case SetOperation::setUnion:
    case SetOperation::setIntersection: {
      auto left = getSaturatedEventBasePairs(Annotated::getLeft(annotatedSet));
      auto right = getSaturatedEventBasePairs(Annotated::getRightSet(annotatedSet));
      left.insert(right.begin(), right.end());
      return left;
    }
    case SetOperation::domain:
    case SetOperation::image: {
      if (set->leftOperand->operation == SetOperation::event &&
          set->relation->operation == RelationOperation::baseRelation) {
        return (annotation->hasValue() && !annotation->getValue().empty()) ? SetOfSets{}
                                                                           : SetOfSets{set};
      }
      return getSaturatedEventBasePairs(Annotated::getLeft(annotatedSet));
    }
    case SetOperation::baseSet:
    case SetOperation::emptySet:
    case SetOperation::fullSet:
    case SetOperation::event:
      return {};
    default:
      throw std::logic_error("unreachable");
  }
}

SetOfSets Literal::saturatedEventBasePairs() const {
  switch (operation) {
    case PredicateOperation::constant:
    case PredicateOperation::equality:
    case PredicateOperation::set:
      return {};
    case PredicateOperation::setNonEmptiness: {
      return getSaturatedEventBasePairs(annotatedSet());
    }
    case PredicateOperation::incomingEdge:
    case PredicateOperation::outgoingEdge:
    case PredicateOperation::edge: {
      if (annotation->hasValue() && !annotation->getValue().empty()) {
        // could be saturated
        return {};
      }
      return eventBasePairs();
    }
    default:
      throw std::logic_error("unreachable");
  }
}

std::optional<Literal> Literal::substituteAll(const CanonicalSet search,
                                              const CanonicalSet replace) const {
  assert(negated);

  switch (operation) {
    case PredicateOperation::setNonEmptiness: {
      const auto newSet = ::substituteAll(annotatedSet(), search, replace);
      if (newSet.first != set) {
        return Literal::newSetNonEmptiness(negated, newSet.first, newSet.second);
      }
      return std::nullopt;
    }
    case PredicateOperation::constant:
      return std::nullopt;
    case PredicateOperation::equality:
    case PredicateOperation::edge: {
      assert(annotation->isLeaf() && annotation->hasValue());
      if (!search->isEvent() || !replace->isEvent()) {
        return std::nullopt;
      }
      const auto left = leftEvent == search ? replace : leftEvent;
      const auto right = rightEvent == search ? replace : rightEvent;

      if (left != leftEvent || right != rightEvent) {
        return newRelationMembership(negated, left, right, identifier.value(), annotation);
      }
      return std::nullopt;
    }
    case PredicateOperation::set: {
      if (!search->isEvent() || !replace->isEvent()) {
        return std::nullopt;
      }
      const auto left = leftEvent == search ? replace : leftEvent;

      if (left != leftEvent) {
        return newSetMembership(negated, left, identifier.value(), annotation);
      }
      return std::nullopt;
    }
    case PredicateOperation::incomingEdge:
    case PredicateOperation::outgoingEdge:
    default:
      throw std::logic_error("unreachable");
  }
}

std::optional<Literal> Literal::substituteAll(const CanonicalRelation search,
                                              const CanonicalRelation replace) const {
  assert(negated);

  switch (operation) {
    case PredicateOperation::setNonEmptiness: {
      const auto newSet = ::substituteAll(annotatedSet(), search, replace);
      if (newSet.first != set) {
        return newSetNonEmptiness(negated, newSet.first, newSet.second);
      }
      return std::nullopt;
    }
    case PredicateOperation::constant:
    case PredicateOperation::equality:
    case PredicateOperation::set:
      return std::nullopt;
    case PredicateOperation::incomingEdge:
    case PredicateOperation::outgoingEdge:
    case PredicateOperation::edge:
      throw std::logic_error("not implemented");
    default:
      throw std::logic_error("unreachable");
  }
}

bool Literal::substitute(const CanonicalSet search, const CanonicalSet replace, int n) {
  switch (operation) {
    case PredicateOperation::constant:
      return false;
    case PredicateOperation::setNonEmptiness: {
      const auto [subSet, subAnnotation] = ::substitute(annotatedSet(), search, replace, &n);
      if (subSet != set) {
        set = subSet;
        annotation = subAnnotation;
        return true;
      }
      return false;
    }
    case PredicateOperation::edge:
    case PredicateOperation::equality:
    case PredicateOperation::set:
    case PredicateOperation::incomingEdge:
    case PredicateOperation::outgoingEdge:
    default:
      throw std::logic_error("unreachable");
  }
}

Literal Literal::substituteSet(const LeafAnnotatedSet<Reasons> &set) const {
  assert(operation == PredicateOperation::setNonEmptiness);
  return newSetNonEmptiness(negated, set.first, set.second);
}

// saturation should return
// - original expression with saturation bund := 0
// - satruated expression with saturation bound -= 1
Cube Literal::saturate() const {
  if (Assumption::baseAssumptions.empty() && Assumption::baseSetAssumptions.empty() &&
      Assumption::idAssumptions.empty()) {
    return {};
  }

  // TODO: we dont add this here but remove the annotation after saturation
  // add same literal without any annotation
  // add literal with one saturated occurrence
  // auto litWithNoAnnotation = *this;
  // litWithNoAnnotation.annotation = LeafAnnotation<Reasons>::newLeaf({});

  Cube saturatedLiterals;  // = {std::move(litWithNoAnnotation)};
  auto cube = Rules::saturate(*this);
  moveAppend(saturatedLiterals, std::move(cube));

  // remove duplicates (saturation methods do not need to check for duplicates)
  removeDuplicates(
      saturatedLiterals);  // TODO: is this still necessasry? (i moved litWithNoAnnotation)
  return saturatedLiterals;
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
    case PredicateOperation::incomingEdge:
    case PredicateOperation::outgoingEdge:
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
    case PredicateOperation::incomingEdge:
      output += identifier->get() + "(*" + "," + leftEvent->toString() + ")";
      break;
    case PredicateOperation::outgoingEdge:
      output += identifier->get() + "(" + leftEvent->toString() + ",*)";
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
