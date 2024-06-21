#pragma once
#include <optional>
#include <string>
#include <unordered_map>

#include "Relation.h"

enum AssumptionType { regular, empty, identity };

class Assumption {
 public:
  Assumption(AssumptionType type, CanonicalRelation relation,
             std::optional<std::string> baseRelation = std::nullopt);

  AssumptionType type;                            // FIXME: Unused
  CanonicalRelation const relation;               // regular, empty, identity
  const std::optional<std::string> baseRelation;  // regular

  static CanonicalRelation masterIdRelation() {
    // construct master identity CanonicalRelation
    CanonicalRelation masterId = Relation::newRelation(RelationOperation::identity);
    for (const auto &assumption : idAssumptions) {
      masterId = Relation::newRelation(RelationOperation::choice, masterId, assumption.relation);
    }
    return masterId;
  }

  static std::vector<Assumption> emptinessAssumptions;
  static std::vector<Assumption> idAssumptions;
  static std::unordered_map<std::string, Assumption> baseAssumptions;
};
