#include <spdlog/spdlog.h>

#include <cassert>
#include <unordered_set>

#include "Annotation.h"
#include "Assumption.h"
#include "Literal.h"
#include "utility.h"

namespace {
// ---------------------- Anonymous helper functions ----------------------

// initialization helper
bool calcIsNormal(const SetOperation operation, const CanonicalSet leftOperand,
                  const CanonicalSet rightOperand, const CanonicalRelation relation) {
  switch (operation) {
    case SetOperation::topEvent:
    case SetOperation::event:
      return true;
    case SetOperation::setUnion:
    case SetOperation::emptySet:
    case SetOperation::fullSet:
    case SetOperation::baseSet:
      return false;
    case SetOperation::setIntersection:
      assert(rightOperand != nullptr);
      return leftOperand->isNormal() && rightOperand->isNormal();
    case SetOperation::domain:
    case SetOperation::image:
      return leftOperand->isEvent() ? relation->operation == RelationOperation::baseRelation
                                    : leftOperand->isNormal();
    default:
      throw std::logic_error("unreachable");
  }
}

EventSet calcEvents(const SetOperation operation, const CanonicalSet leftOperand,
                    const CanonicalSet rightOperand, const std::optional<int> label) {
  switch (operation) {
    case SetOperation::setIntersection:
    case SetOperation::setUnion: {
      auto leftLabels = leftOperand->getEvents();
      auto rightLabels = rightOperand->getEvents();
      leftLabels.insert(rightLabels.begin(), rightLabels.end());
      return leftLabels;
    }
    case SetOperation::domain:
    case SetOperation::image:
      return leftOperand->getEvents();
    case SetOperation::event:
      return {*label};
    case SetOperation::topEvent:
    case SetOperation::baseSet:
    case SetOperation::emptySet:
    case SetOperation::fullSet:
      return {};
    default:
      throw std::logic_error("unreachable");
  }
}

EventSet calcTopEvents(const SetOperation operation, const CanonicalSet leftOperand,
                       const CanonicalSet rightOperand, const std::optional<int> label) {
  switch (operation) {
    case SetOperation::setIntersection:
    case SetOperation::setUnion: {
      auto leftEvents = leftOperand->getTopEvents();
      auto rightEvents = rightOperand->getTopEvents();
      leftEvents.insert(rightEvents.begin(), rightEvents.end());
      return leftEvents;
    }
    case SetOperation::domain:
    case SetOperation::image:
      return leftOperand->getTopEvents();
    case SetOperation::topEvent:
      return {*label};
    case SetOperation::event:
    case SetOperation::baseSet:
    case SetOperation::emptySet:
    case SetOperation::fullSet:
      return {};
    default:
      throw std::logic_error("unreachable");
  }
}

SetOfSets calcLabelBaseCombinations(const SetOperation operation, const CanonicalSet leftOperand,
                                    const CanonicalSet rightOperand,
                                    const CanonicalRelation relation, const CanonicalSet thisRef) {
  switch (operation) {
    case SetOperation::setUnion:
    case SetOperation::setIntersection: {
      auto left = leftOperand->getLabelBaseCombinations();
      auto right = rightOperand->getLabelBaseCombinations();
      left.insert(right.begin(), right.end());
      return left;
    }
    case SetOperation::domain:
    case SetOperation::image: {
      return leftOperand->operation == SetOperation::event &&
                     relation->operation == RelationOperation::baseRelation
                 ? SetOfSets{thisRef}
                 : leftOperand->getLabelBaseCombinations();
    }
    case SetOperation::baseSet:
    case SetOperation::emptySet:
    case SetOperation::fullSet:
    case SetOperation::topEvent:
    case SetOperation::event:
      return {};
    default:
      throw std::logic_error("unreachable");
  }
}

}  // namespace

int Set::maxEvent = 0;

void Set::completeInitialization() const {
  this->_isNormal = calcIsNormal(operation, leftOperand, rightOperand, relation);
  this->topEvents = calcTopEvents(operation, leftOperand, rightOperand, label);
  this->events = calcEvents(operation, leftOperand, rightOperand, label);
  this->eventRelationCombinations =
      calcLabelBaseCombinations(operation, leftOperand, rightOperand, relation, this);
}

Set::Set(const SetOperation operation, const CanonicalSet left, const CanonicalSet right,
         const CanonicalRelation relation, const std::optional<int> label,
         std::optional<std::string> identifier)
    : operation(operation),
      identifier(std::move(identifier)),
      label(label),
      leftOperand(left),
      rightOperand(right),
      relation(relation) {}

CanonicalSet Set::newSet(const SetOperation operation, const CanonicalSet left,
                         const CanonicalSet right, const CanonicalRelation relation,
                         const std::optional<int> label,
                         const std::optional<std::string> &identifier) {
#if (DEBUG)
  // ------------------ Validation ------------------
  const bool isSimple = (left == nullptr && right == nullptr && relation == nullptr);
  const bool hasLabelOrId = (label.has_value() || identifier.has_value());
  switch (operation) {
    case SetOperation::baseSet:
      assert(identifier.has_value() && !label.has_value() && isSimple);
      break;
    case SetOperation::topEvent:
      assert(label.has_value() && !identifier.has_value() && isSimple);
      break;
    case SetOperation::event:
      assert(label.has_value() && !identifier.has_value() && isSimple);
      break;
    case SetOperation::emptySet:
    case SetOperation::fullSet:
      assert(!hasLabelOrId && isSimple);
      break;
    case SetOperation::setUnion:
    case SetOperation::setIntersection:
      assert(!hasLabelOrId);
      assert(left != nullptr && right != nullptr && relation == nullptr);
      break;
    case SetOperation::image:
    case SetOperation::domain:
      assert(!hasLabelOrId);
      assert(left != nullptr && relation != nullptr && right == nullptr);
      break;
    default:
      assert(false);
      throw std::logic_error("unreachable");
  }
#endif
  static std::unordered_set<Set> canonicalizer;
  auto [iter, created] =
    canonicalizer.insert(std::move(Set(operation, left, right, relation, label, identifier)));
  //= canonicalizer.emplace(operation, left, right, relation, label, identifier);
  if (created) {
    iter->completeInitialization();
  }
  return &*iter;
}

bool Set::operator==(const Set &other) const {
  return operation == other.operation && leftOperand == other.leftOperand &&
         rightOperand == other.rightOperand && relation == other.relation && label == other.label &&
         identifier == other.identifier;
}

CanonicalSet Set::rename(const Renaming &renaming) const {
  CanonicalSet leftRenamed;
  CanonicalSet rightRenamed;
  switch (operation) {
    case SetOperation::topEvent:
      return newTopEvent(renaming.rename(label.value()));
    case SetOperation::event:
      return newEvent(renaming.rename(label.value()));
    case SetOperation::baseSet:
    case SetOperation::emptySet:
    case SetOperation::fullSet:
      return this;
    case SetOperation::setUnion:
    case SetOperation::setIntersection:
      leftRenamed = leftOperand->rename(renaming);
      rightRenamed = rightOperand->rename(renaming);
      return newSet(operation, leftRenamed, rightRenamed);
    case SetOperation::image:
    case SetOperation::domain:
      leftRenamed = leftOperand->rename(renaming);
      return newSet(operation, leftRenamed, relation);
    default:
      throw std::logic_error("unreachable");
  }
}

std::string Set::toString() const {
  if (cachedStringRepr) {
    return *cachedStringRepr;
  }
  std::string output;
  switch (operation) {
    case SetOperation::topEvent:
      output += "[" + std::to_string(*label) + "]";
      break;
    case SetOperation::event:
      output += std::to_string(*label);
      break;
    case SetOperation::image:
      output += "(" + leftOperand->toString() + ";" + relation->toString() + ")";
      break;
    case SetOperation::domain:
      output += "(" + relation->toString() + ";" + leftOperand->toString() + ")";
      break;
    case SetOperation::baseSet:
      output += *identifier;
      break;
    case SetOperation::emptySet:
      output += "0";
      break;
    case SetOperation::fullSet:
      output += "T";
      break;
    case SetOperation::setIntersection:
      output += "(" + leftOperand->toString() + " & " + rightOperand->toString() + ")";
      break;
    case SetOperation::setUnion:
      output += "(" + leftOperand->toString() + " | " + rightOperand->toString() + ")";
      break;
    default:
      throw std::logic_error("unreachable");
  }
  cachedStringRepr.emplace(std::move(output));
  return *cachedStringRepr;
}
