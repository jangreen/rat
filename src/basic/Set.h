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
  baseSet,          // nullary function (constant): base Set
  event,            // nullary function (constant): single Set
  emptySet,         // nullary function (constant): empty Set
  fullSet,          // nullary function (constant): full Set
  setUnion,         // binary function
  setIntersection,  // binary function
  image,            // binary function
  domain
};

class Set {
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
  mutable SetOfSets baseSets;

  // Calculates the above properties: we do not do this inside the constructor
  //  to avoid doing it for non-canonical sets.
  void completeInitialization() const;

  static int maxEvent;  // to create globally unique events

 public:
  // WARNING: Never call these constructors: they are only public for technical reasons
  // Due to canonicalization, moving or copying is not allowed
  Set(const Set &other) = default;
  // Set(const Set &&other) = default;

  static CanonicalSet emptySet();
  static CanonicalSet fullSet();
  static CanonicalSet newBaseSet(const std::string &identifier);
  static CanonicalSet newEvent(int label);
  static CanonicalSet newSet(SetOperation operation, CanonicalSet left, CanonicalSet right);
  static CanonicalSet newSet(SetOperation operation, CanonicalSet left, CanonicalRelation relation);
  static CanonicalSet freshEvent();

  [[nodiscard]] bool operator==(const Set &other) const;

  bool isEvent() const { return operation == SetOperation::event; }
  const bool &isNormal() const { return _isNormal; }
  bool hasFullSet() const { return _hasFullSet; }
  bool hasBaseSet() const { return _hasBaseSet; }
  const EventSet &getEvents() const { return events; }
  const SetOfSets &getEventBasePairs() const { return eventBasePairs; }
  const SetOfSets &getBaseSets() const { return baseSets; }
  const EventSet &getNormalEvents() const { return normalEvents; }
  [[nodiscard]] CanonicalSet intersectWith(CanonicalSet other) const;
  [[nodiscard]] CanonicalSet imageWith(CanonicalRelation other) const;
  [[nodiscard]] CanonicalSet domainWith(CanonicalRelation other) const;
  [[nodiscard]] int intersectionWidth() const;
  [[nodiscard]] int compositionLength() const;
  [[nodiscard]] bool isSmallerReason(CanonicalSet other) const;

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