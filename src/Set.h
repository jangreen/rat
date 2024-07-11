#pragma once
#include <boost/container/flat_set.hpp>
#include <optional>

#include "Relation.h"
#include "Renaming.h"
#include "unordered_set"

class Set;
typedef const Set *CanonicalSet;
typedef boost ::container::flat_set<int> EventSet;
typedef boost ::container::flat_set<CanonicalSet> SetOfSets;

enum class SetOperation {
  baseSet,          // nullary function (constant): base Set
  event,            // nullary function (constant): single Set
  topEvent,         // special event for lazy top evaluation
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
  // Due to canonicalization, moving or copying is not allowed
  static CanonicalSet newSet(SetOperation operation, CanonicalSet left, CanonicalSet right,
                             CanonicalRelation relation, std::optional<int> label,
                             const std::optional<std::string> &identifier);

  // Cached values
  mutable std::optional<std::string> cachedStringRepr;

  // properties calculated for canonical sets on initialization
  mutable bool _isNormal{};
  mutable EventSet topEvents;
  mutable EventSet events;
  mutable EventSet normalEvents;
  mutable SetOfSets eventRelationCombinations;

  // Calculates the above properties: we do not do this inside the constructor
  //  to avoid doing it for non-canonical sets.
  void completeInitialization() const;

 public:
  // WARNING: Never call these constructors: they are only public for technical reasons
  Set(const Set &other) = default;
  // Set(const Set &&other) = default;

  static int maxEvent;  // to create globally unique events
  inline static CanonicalSet emptySet() {
    return newSet(SetOperation::emptySet, nullptr, nullptr, nullptr, std::nullopt, std::nullopt);
  }
  inline static CanonicalSet fullSet() {
    return newSet(SetOperation::fullSet, nullptr, nullptr, nullptr, std::nullopt, std::nullopt);
  }
  inline static CanonicalSet newBaseSet(std::string &identifier) {
    return newSet(SetOperation::baseSet, nullptr, nullptr, nullptr, std::nullopt, identifier);
  }
  inline static CanonicalSet newEvent(int label) {
    return newSet(SetOperation::event, nullptr, nullptr, nullptr, label, std::nullopt);
  };
  inline static CanonicalSet newTopEvent(int label) {
    return newSet(SetOperation::topEvent, nullptr, nullptr, nullptr, label, std::nullopt);
  };
  inline static CanonicalSet newSet(SetOperation operation, CanonicalSet left, CanonicalSet right) {
    return newSet(operation, left, right, nullptr, std::nullopt, std::nullopt);
  }
  inline static CanonicalSet newSet(SetOperation operation, CanonicalSet left,
                                    CanonicalRelation relation) {
    return newSet(operation, left, nullptr, relation, std::nullopt, std::nullopt);
  }

  bool operator==(const Set &other) const;

  inline const bool isEvent() const {
    return operation == SetOperation::event || operation == SetOperation::topEvent;
  };
  inline const bool &isNormal() const { return _isNormal; }
  inline const bool hasTopEvent() const { return !topEvents.empty(); }
  inline const EventSet &getTopEvents() const { return topEvents; }
  inline const EventSet &getEvents() const { return events; }
  inline const EventSet &getNormalEvents() const { return normalEvents; }
  inline const SetOfSets &getLabelBaseCombinations() const { return eventRelationCombinations; }

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