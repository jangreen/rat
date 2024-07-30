#include <boost/unordered/unordered_node_set.hpp>
#include <cassert>

#include "../Assumption.h"
#include "../utility.h"
#include "Annotation.h"
#include "Literal.h"

namespace {
// ---------------------- Anonymous helper functions ----------------------

// initialization helper
bool calcIsNormal(const SetOperation operation, const CanonicalSet leftOperand,
                  const CanonicalSet rightOperand, const CanonicalRelation relation) {
  switch (operation) {
    // TODO (topEvent optimization): case SetOperation::topEvent:
    case SetOperation::fullSet:
    case SetOperation::baseSet:
      return true;
    case SetOperation::event:
    case SetOperation::setUnion:
    case SetOperation::emptySet:
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
      return {label.value()};
    // TODO (topEvent optimization): case SetOperation::topEvent:
    case SetOperation::baseSet:
    case SetOperation::emptySet:
    case SetOperation::fullSet:
      return {};
    default:
      throw std::logic_error("unreachable");
  }
}

EventSet calcNormalEvents(const SetOperation operation, const CanonicalSet leftOperand,
                          const CanonicalSet rightOperand, const CanonicalRelation relation) {
  switch (operation) {
    case SetOperation::setIntersection: {
      auto leftLabels = leftOperand->getNormalEvents();
      auto rightLabels = rightOperand->getNormalEvents();
      leftLabels.insert(rightLabels.begin(), rightLabels.end());
      return leftLabels;
    }
    case SetOperation::domain:
    case SetOperation::image:
      if (leftOperand->operation == SetOperation::event &&
          relation->operation == RelationOperation::baseRelation) {
        return {leftOperand->label.value()};
      }
      return leftOperand->getNormalEvents();
    case SetOperation::setUnion:
    case SetOperation::event:
    // TODO (topEvent optimization): case SetOperation::topEvent:
    case SetOperation::baseSet:
    case SetOperation::emptySet:
    case SetOperation::fullSet:
      return {};
    default:
      throw std::logic_error("unreachable");
  }
}

// TODO (topEvent optimization):
// EventSet calcTopEvents(const SetOperation operation, const CanonicalSet leftOperand,
//                        const CanonicalSet rightOperand, const std::optional<int> label) {
//   switch (operation) {
//     case SetOperation::setIntersection:
//     case SetOperation::setUnion: {
//       auto leftEvents = leftOperand->getTopEvents();
//       auto rightEvents = rightOperand->getTopEvents();
//       leftEvents.insert(rightEvents.begin(), rightEvents.end());
//       return leftEvents;
//     }
//     case SetOperation::domain:
//     case SetOperation::image:
//       return leftOperand->getTopEvents();
//     case SetOperation::topEvent:
//       return {*label};
//     case SetOperation::event:
//     case SetOperation::baseSet:
//     case SetOperation::emptySet:
//     case SetOperation::fullSet:
//       return {};
//     default:
//       throw std::logic_error("unreachable");
//   }
// }

bool calcHasFullSet(const SetOperation operation, const CanonicalSet leftOperand,
                    const CanonicalSet rightOperand) {
  switch (operation) {
    case SetOperation::setIntersection:
    case SetOperation::setUnion:
      return leftOperand->hasFullSet() || rightOperand->hasFullSet();
    case SetOperation::domain:
    case SetOperation::image:
      return leftOperand->hasFullSet();
    case SetOperation::event:
    case SetOperation::baseSet:
    case SetOperation::emptySet:
      return false;
    case SetOperation::fullSet:
      return true;
    default:
      throw std::logic_error("unreachable");
  }
}

bool calcHasBaseSet(const SetOperation operation, const CanonicalSet leftOperand,
                    const CanonicalSet rightOperand) {
  switch (operation) {
    case SetOperation::event:
    case SetOperation::fullSet:
    case SetOperation::emptySet:
      return false;
    case SetOperation::baseSet:
      return true;
    case SetOperation::setIntersection:
    case SetOperation::setUnion:
      return leftOperand->hasBaseSet() || rightOperand->hasBaseSet();
    case SetOperation::domain:
    case SetOperation::image:
      return leftOperand->hasBaseSet();
    default:
      throw std::logic_error("unreachable");
  }
}

SetOfSets calcEventBasePairs(const SetOperation operation, const CanonicalSet leftOperand,
                             const CanonicalSet rightOperand, const CanonicalRelation relation,
                             const CanonicalSet thisRef) {
  switch (operation) {
    case SetOperation::setUnion:
    case SetOperation::setIntersection: {
      auto left = leftOperand->getEventBasePairs();
      auto right = rightOperand->getEventBasePairs();
      left.insert(right.begin(), right.end());
      return left;
    }
    case SetOperation::domain:
    case SetOperation::image: {
      return leftOperand->operation == SetOperation::event &&
                     relation->operation == RelationOperation::baseRelation
                 ? SetOfSets{thisRef}
                 : leftOperand->getEventBasePairs();
    }
    case SetOperation::baseSet:
    case SetOperation::emptySet:
    case SetOperation::fullSet:
    // TODO (topEvent optimization): case SetOperation::topEvent:
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
  // TODO (topEvent optimization): this->topEvents = calcTopEvents(operation, leftOperand,
  // rightOperand, label);
  this->_hasFullSet = calcHasFullSet(operation, leftOperand, rightOperand);
  this->_hasBaseSet = calcHasBaseSet(operation, leftOperand, rightOperand);
  this->events = calcEvents(operation, leftOperand, rightOperand, label);
  this->normalEvents = calcNormalEvents(operation, leftOperand, rightOperand, relation);
  this->eventBasePairs = calcEventBasePairs(operation, leftOperand, rightOperand, relation, this);
  if constexpr (DEBUG) {
    // To populate the cache for better debugging
    std::ignore = toString();
  }
}

Set::Set(const SetOperation operation, const CanonicalSet left, const CanonicalSet right,
         const CanonicalRelation relation, const std::optional<int> label,
         std::optional<std::string> identifier)
    : _isNormal(false),
      _hasFullSet(false),
      _hasBaseSet(false),
      operation(operation),
      identifier(std::move(identifier)),
      label(label),
      leftOperand(left),
      rightOperand(right),
      relation(relation) {}

CanonicalSet Set::newSet(const SetOperation operation, const CanonicalSet left,
                         const CanonicalSet right, const CanonicalRelation relation,
                         const std::optional<int> label,
                         const std::optional<std::string> &identifier) {
  if constexpr (DEBUG) {
    // ------------------ Validation ------------------
    const bool isSimple = (left == nullptr && right == nullptr && relation == nullptr);
    const bool hasLabelOrId = (label.has_value() || identifier.has_value());
    switch (operation) {
      case SetOperation::baseSet:
        assert(identifier.has_value() && !label.has_value() && isSimple);
        break;
      // TODO (topEvent optimization):
      // case SetOperation::topEvent:
      //   assert(label.has_value() && !identifier.has_value() && isSimple);
      //   break;
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
  }
  static boost::unordered::unordered_node_set<Set, std::hash<Set>> canonicalizer;
  auto [iter, created] =
      canonicalizer.insert(std::move(Set(operation, left, right, relation, label, identifier)));
  if (created) {
    iter->completeInitialization();
  }

  /*
  static size_t attempts = 0;
  if (++attempts % 1000000 == 0) {
    std::cout << attempts << "\n";
  }
  if (canonicalizer.size() % 500 == 0) {
    auto [total, max] = countCollisions(canonicalizer);
    std::cout << "#Entries/#Buckets/#Collisions/#MaxCollision: "
    << canonicalizer.size()
    << "/" << canonicalizer.bucket_count()
    << "/" << total
    << "/" << max << "\n";
    std::cout << "Badness factor: " << measure_unordered_badness(canonicalizer) << "\n";
  }*/
  return &*iter;
}

CanonicalSet Set::rename(const Renaming &renaming) const {
  CanonicalSet leftRenamed;
  CanonicalSet rightRenamed;
  switch (operation) {
    // TODO (topEvent optimization):
    // case SetOperation::topEvent: {
    //   const int renamed = renaming.rename(label.value());
    //   return label.value() == renamed ? this : newTopEvent(renaming.rename(label.value()));
    // }
    case SetOperation::event: {
      const int renamed = renaming.rename(label.value());
      return label.value() == renamed ? this : newEvent(renaming.rename(label.value()));
    }
    case SetOperation::baseSet:
    case SetOperation::emptySet:
    case SetOperation::fullSet:
      return this;
    case SetOperation::setUnion:
    case SetOperation::setIntersection:
      leftRenamed = leftOperand->rename(renaming);
      rightRenamed = rightOperand->rename(renaming);
      if (leftRenamed == leftOperand && rightRenamed == rightOperand) {
        return this;
      }
      return newSet(operation, leftRenamed, rightRenamed);
    case SetOperation::image:
    case SetOperation::domain:
      leftRenamed = leftOperand->rename(renaming);
      return leftRenamed == leftOperand ? this : newSet(operation, leftRenamed, relation);
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
    // TODO (topEvent optimization):
    // case SetOperation::topEvent:
    //   output += "[" + std::to_string(*label) + "]";
    //   break;
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
