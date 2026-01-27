#pragma once
#include <unordered_set>
#include <unordered_map>

#include "../basic/Relation.h"

struct Assumptions {
  std::unordered_set<CanonicalRelation> emptinessAssumptions{};
  std::unordered_set<CanonicalRelation> idAssumptions{};
  std::unordered_map<std::string, CanonicalRelation> baseAssumptions{};
  std::unordered_set<CanonicalSet> setEmptinessAssumptions{};
  std::unordered_map<std::string, CanonicalSet> baseSetAssumptions{};
};