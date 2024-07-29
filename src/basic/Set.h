#pragma once
#include <boost/container/flat_set.hpp>
#include <boost/container_hash/hash.hpp>
#include <optional>

#include "Relation.h"
#include "Renaming.h"

class Set;
typedef const Set *CanonicalSet;
typedef boost::container::flat_set<int> EventSet;
typedef boost::container::flat_set<CanonicalSet> SetOfSets;

enum class SetOperation {
  baseSet,  // nullary function (constant): base Set
  event,    // nullary function (constant): single Set
  // TODO (topEvent optimization): topEvent,         // special event for lazy top evaluation
  emptySet,         // nullary function (constant): empty Set
  fullSet,          // nullary function (constant): full Set
  setUnion,         // binary function
  setIntersection,  // binary function
  image,            // binary function
  domain
};

class Set {
 private:
  Set(SetOperation operation, CanonicalSet left, CanonicalSet right, CanonicalRelation relation,
      std::optional<int> label, std::optional<std::string> identifier);
  static CanonicalSet newSet(SetOperation operation, CanonicalSet left, CanonicalSet right,
                             CanonicalRelation relation, std::optional<int> label,
                             const std::optional<std::string> &identifier);

  // ================ Cached values ================
  mutable std::optional<std::string> cachedStringRepr;
  // properties calculated for canonical sets on initialization
  mutable bool _isNormal;
  mutable bool _hasFullSet;
  mutable bool _hasBaseSet;
  mutable EventSet events;
  mutable EventSet normalEvents;
  mutable SetOfSets eventBasePairs;
  // TODO (topEvent optimization): mutable EventSet topEvents;

  // Calculates the above properties: we do not do this inside the constructor
  //  to avoid doing it for non-canonical sets.
  void completeInitialization() const;

  static int maxEvent;  // to create globally unique events

 public:
  // WARNING: Never call these constructors: they are only public for technical reasons
  // Due to canonicalization, moving or copying is not allowed
  Set(const Set &other) = default;
  // Set(const Set &&other) = default;

  static CanonicalSet emptySet() {
    return newSet(SetOperation::emptySet, nullptr, nullptr, nullptr, std::nullopt, std::nullopt);
  }
  static CanonicalSet fullSet() {
    return newSet(SetOperation::fullSet, nullptr, nullptr, nullptr, std::nullopt, std::nullopt);
  }
  static CanonicalSet newBaseSet(const std::string &identifier) {
    return newSet(SetOperation::baseSet, nullptr, nullptr, nullptr, std::nullopt, identifier);
  }
  static CanonicalSet newEvent(int label) {
    return newSet(SetOperation::event, nullptr, nullptr, nullptr, label, std::nullopt);
  }
  // TODO (topEvent optimization):
  // static CanonicalSet newTopEvent(int label) {
  //   return newSet(SetOperation::topEvent, nullptr, nullptr, nullptr, label, std::nullopt);
  // }
  static CanonicalSet newSet(SetOperation operation, CanonicalSet left, CanonicalSet right) {
    return newSet(operation, left, right, nullptr, std::nullopt, std::nullopt);
  }
  static CanonicalSet newSet(SetOperation operation, CanonicalSet left,
                             CanonicalRelation relation) {
    return newSet(operation, left, nullptr, relation, std::nullopt, std::nullopt);
  }
  static CanonicalSet freshEvent() { return newEvent(maxEvent++); }
  // TODO (topEvent optimization):
  // static CanonicalSet freshTopEvent() { return newTopEvent(maxEvent++); }

  bool operator==(const Set &other) const {
    return operation == other.operation && leftOperand == other.leftOperand &&
           rightOperand == other.rightOperand && relation == other.relation &&
           label == other.label && identifier == other.identifier;
  }

  bool isEvent() const {
    return operation == SetOperation::event;  // TODO (topEvent optimization): || operation ==
                                              // SetOperation::topEvent;
  }
  const bool &isNormal() const { return _isNormal; }
  bool hasFullSet() const { return _hasFullSet; }
  bool hasBaseSet() const { return _hasBaseSet; }
  // TODO (topEvent optimization): bool hasTopEvent() const { return !topEvents.empty(); }
  // TODO (topEvent optimization): const EventSet &getTopEvents() const { return topEvents; }
  const EventSet &getEvents() const { return events; }
  const SetOfSets &getEventBasePairs() const { return eventBasePairs; }
  const EventSet &getNormalEvents() const { return normalEvents; }
  // TODO (topEvent optimization): bool hasTopEvent() const { return !topEvents.empty(); }
  // TODO (topEvent optimization): const EventSet &getTopEvents() const { return topEvents; }

  const SetOperation operation;
  const std::optional<std::string> identifier;  // is set iff operation base
  const std::optional<int> label;               // is set iff operation event
  const CanonicalSet leftOperand;               // is set iff operation unary/binary
  const CanonicalSet rightOperand;              // is set iff operation binary
  const CanonicalRelation relation;             // is set iff domain/image

  [[nodiscard]] CanonicalSet rename(const Renaming &renaming) const;

  // printing
  [[nodiscard]] std::string toString() const;
};

template <>
struct std::hash<SetOperation> {
  std::size_t operator()(const SetOperation &operation) const noexcept {
    return static_cast<std::size_t>(operation);
  }
};

template <>
struct std::hash<Set> {
  std::size_t operator()(const Set &set) const noexcept {
    size_t seed = 31;
    boost::hash_combine(seed, set.operation);
    boost::hash_combine(seed, set.leftOperand);
    boost::hash_combine(seed, set.rightOperand);
    boost::hash_combine(seed, set.relation);
    boost::hash_combine(seed, set.identifier);
    boost::hash_combine(seed, set.label);
    return seed;
  }
};