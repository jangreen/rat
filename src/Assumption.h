#pragma once
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "basic/Relation.h"
#include "basic/Set.h"

class Assumption {
 public:
  explicit Assumption(CanonicalRelation relation,
                      std::optional<std::string> baseRelation = std::nullopt);
  explicit Assumption(CanonicalSet set, std::optional<std::string> baseRelation = std::nullopt);

  const CanonicalRelation relation;                 // regular, empty, identity
  const CanonicalSet set;                           // regular, empty
  const std::optional<std::string> baseIdentifier;  // regular

  static CanonicalRelation masterIdRelation() {
    // construct master identity CanonicalRelation
    CanonicalRelation masterId = nullptr;
    for (const auto &assumption : idAssumptions) {
      masterId = masterId == nullptr ? assumption.relation
                                     : Relation::newRelation(RelationOperation::relationUnion,
                                                             masterId, assumption.relation);
    }
    const auto closure = Relation::newRelation(RelationOperation::transitiveClosure, masterId);
    return closure;
  }

  static std::vector<Assumption> emptinessAssumptions;
  static std::vector<Assumption> idAssumptions;
  static std::unordered_map<std::string, Assumption> baseAssumptions;
  static std::vector<Assumption> setEmptinessAssumptions;
  static std::unordered_map<std::string, Assumption> baseSetAssumptions;
};
