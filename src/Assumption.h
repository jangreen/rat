#pragma once
#include <string>
#include <optional>
#include "Relation.h"

enum class AssumptionType
{
    regular,
    empty,
    identity
};

class Assumption
{
public:
    Assumption(const AssumptionType type, Relation &&relation, std::optional<std::string> baseRelation = std::nullopt);

    AssumptionType type;
    Relation relation;
    std::optional<std::string> baseRelation; // is set iff regular
};
