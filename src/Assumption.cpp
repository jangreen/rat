#include "Assumption.h"

Assumption::Assumption(const CanonicalRelation relation, std::optional<std::string> baseRelation)
    : relation(relation), baseRelation(std::move(baseRelation)) {}

std::vector<Assumption> Assumption::emptinessAssumptions;
std::vector<Assumption> Assumption::idAssumptions;
std::unordered_map<std::string, Assumption> Assumption::baseAssumptions;