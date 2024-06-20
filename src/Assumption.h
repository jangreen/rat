#pragma once
#include <optional>
#include <string>
#include <unordered_map>

#include "Relation.h"

enum class AssumptionType { regular, empty, identity };

class Assumption {
 public:
  Assumption(const AssumptionType type, CanonicalRelation relation,
             std::optional<std::string> baseRelation = std::nullopt);

  AssumptionType type;
  CanonicalRelation const relation;               // regular, empty, idententity
  const std::optional<std::string> baseRelation;  // regular

  static CanonicalRelation masterIdRelation() {
    // construct master identity CanonicalRelation
    CanonicalRelation masterId = CanonicalRelation(RelationOperation::identity);
    for (const auto assumption : idAssumptions) {
      masterId = Relation::newRelation(RelationOperation::choice, masterId, assumption.relation);
    }
    return masterId;
  }

  static std::vector<Assumption> emptinessAssumptions;
  static std::vector<Assumption> idAssumptions;
  static std::unordered_map<std::string, Assumption> baseAssumptions;
};
