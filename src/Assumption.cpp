#include "Assumption.h"

Assumption::Assumption(const AssumptionType type, CanonicalRelation relation,
                       std::optional<std::string> baseRelation)
    : type(type), relation(relation), baseRelation(baseRelation) {}

std::vector<Assumption> Assumption::emptinessAssumptions;
std::vector<Assumption> Assumption::idAssumptions;
std::unordered_map<std::string, Assumption> Assumption::baseAssumptions;