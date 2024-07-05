#pragma once
#include <optional>

#include "Relation.h"
#include "Renaming.h"

class Set;
typedef const Set *CanonicalSet;

enum class SetOperation {
  base,          // nullary function (constant): base Set
  singleton,     // nullary function (constant): single Set
  empty,         // nullary function (constant): empty Set
  full,          // nullary function (constant): full Set
  choice,        // binary function
  intersection,  // binary function
  image,         // binary function
  domain
};

class Set {
 private:
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
  // WARNING: Never call this constructor: it is only public for technicaly reasons
  Set(SetOperation operation, CanonicalSet left, CanonicalSet right, CanonicalRelation relation,
      std::optional<int> label, std::optional<std::string> identifier);

  static int maxSingletonLabel;  // to create globally unique labels
  static CanonicalSet emptySet() {
    return newSet(SetOperation::empty, nullptr, nullptr, nullptr, std::nullopt, std::nullopt);
  };
  static CanonicalSet fullSet() {
    return newSet(SetOperation::full, nullptr, nullptr, nullptr, std::nullopt, std::nullopt);
  };
  static CanonicalSet newSet(SetOperation operation, CanonicalSet left, CanonicalSet right);
  static CanonicalSet newSet(SetOperation operation, CanonicalSet left, CanonicalRelation relation);
  static CanonicalSet newEvent(int label);
  static CanonicalSet newBaseSet(std::string &identifier);

  // Due to canonicalization, moving or copying is not allowed
  Set(const Set &other) = delete;
  Set(const Set &&other) = delete;

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