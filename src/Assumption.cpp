#include "Assumption.h"

#include <utility>

Assumption::Assumption(const AssumptionType type, CanonicalRelation relation,
                       std::optional<std::string> baseRelation)
    : type(type), relation(relation), baseRelation(std::move(baseRelation)) {}

std::vector<Assumption> Assumption::emptinessAssumptions;
std::vector<Assumption> Assumption::idAssumptions;
std::unordered_map<std::string, Assumption> Assumption::baseAssumptions;