#include "Assumption.h"

Assumption::Assumption(const CanonicalRelation relation, std::optional<std::string> baseRelation)
    : relation(relation), baseIdentifier(std::move(baseRelation)), set(nullptr) {}

Assumption::Assumption(const CanonicalSet set, std::optional<std::string> baseRelation)
    : relation(nullptr), baseIdentifier(std::move(baseRelation)), set(set) {}

std::vector<Assumption> Assumption::emptinessAssumptions;
std::vector<Assumption> Assumption::idAssumptions;
std::unordered_map<std::string, Assumption> Assumption::baseAssumptions;
std::vector<Assumption> Assumption::setEmptinessAssumptions;
std::unordered_map<std::string, Assumption> Assumption::baseSetAssumptions;