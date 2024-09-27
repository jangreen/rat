#include "Assumption.h"

#include "basic/CanonicalString.h"

Assumption::Assumption(const CanonicalRelation relation, std::optional<std::string> baseRelation)
    : relation(relation), set(nullptr), baseIdentifier(std::move(baseRelation)) {}

Assumption::Assumption(const CanonicalSet set, std::optional<std::string> baseRelation)
    : relation(nullptr), set(set), baseIdentifier(std::move(baseRelation)) {}

std::vector<Assumption> Assumption::emptinessAssumptions;
std::vector<Assumption> Assumption::idAssumptions;
std::unordered_map<std::string, Assumption> Assumption::baseAssumptions;
std::vector<Assumption> Assumption::setEmptinessAssumptions;
std::unordered_map<std::string, Assumption> Assumption::baseSetAssumptions;