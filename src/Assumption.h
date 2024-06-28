#pragma once
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "Relation.h"
class Assumption {
 public:
  explicit Assumption(CanonicalRelation relation,
                      std::optional<std::string> baseRelation = std::nullopt);

  const CanonicalRelation relation;               // regular, empty, identity
  const std::optional<std::string> baseRelation;  // regular

  static CanonicalRelation masterIdRelation() {
    // construct master identity CanonicalRelation
    CanonicalRelation masterId = nullptr;
    for (const auto &assumption : idAssumptions) {
      masterId = masterId == nullptr ? assumption.relation
                                     : Relation::newRelation(RelationOperation::choice, masterId,
                                                             assumption.relation);
    }
    auto closure = Relation::newRelation(RelationOperation::transitiveClosure, masterId);
    return closure;
  }

  static std::vector<Assumption> emptinessAssumptions;
  static std::vector<Assumption> idAssumptions;
  static std::unordered_map<std::string, Assumption> baseAssumptions;
};
