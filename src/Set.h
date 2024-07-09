#pragma once
#include <optional>

#include "Relation.h"
#include "Renaming.h"
#include "unordered_set"

class Set;
typedef const Set *CanonicalSet;

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
  mutable bool _hasTopSet{};
  mutable std::vector<int> labels;
  mutable std::vector<CanonicalSet> labelBaseCombinations;

  // Calculates the above properties: we do not do this inside the constructor
  //  to avoid doing it for non-canonical sets.
  void completeInitialization() const;

 public:
  // WARNING: Never call these constructors: they are only public for technical reasons
  Set(const Set &other) = default;
  // Set(const Set &&other) = default;

  static int maxSingletonLabel;  // to create globally unique labels
  static CanonicalSet emptySet() {
    return newSet(SetOperation::emptySet, nullptr, nullptr, nullptr, std::nullopt, std::nullopt);
  }
  static CanonicalSet fullSet() {
    return newSet(SetOperation::fullSet, nullptr, nullptr, nullptr, std::nullopt, std::nullopt);
  }
  static CanonicalSet newBaseSet(std::string &identifier) {
    return newSet(SetOperation::baseSet, nullptr, nullptr, nullptr, std::nullopt, identifier);
  }
  static CanonicalSet newEvent(int label) {
    return newSet(SetOperation::event, nullptr, nullptr, nullptr, label, std::nullopt);
  }
  static CanonicalSet newSet(SetOperation operation, CanonicalSet left, CanonicalSet right) {
    return newSet(operation, left, right, nullptr, std::nullopt, std::nullopt);
  }
  static CanonicalSet newSet(SetOperation operation, CanonicalSet left,
                                    CanonicalRelation relation) {
    return newSet(operation, left, nullptr, relation, std::nullopt, std::nullopt);
  }

  bool operator==(const Set &other) const;

  const bool &isNormal() const;
  const bool &hasTopSet() const;
  const std::vector<int> &getLabels() const;
  const std::vector<CanonicalSet> &getLabelBaseCombinations() const;

  const SetOperation operation;
  const std::optional<std::string> identifier;  // is set iff operation base
  const std::optional<int> label;               // is set iff operation singleton
  const CanonicalSet leftOperand;               // is set iff operation unary/binary
  const CanonicalSet rightOperand;              // is set iff operation binary
  const CanonicalRelation relation;             // is set iff domain/image

  [[nodiscard]] CanonicalSet rename(const Renaming &renaming) const;

  // printing
  [[nodiscard]] std::string toString() const;
};