#pragma once
#include <string>
#include "Relation.h"

using namespace std;

enum class AssumptionType
{
    regular,
    empty,
    identity
};

class Assumption
{
public:
    Assumption(const AssumptionType type, Relation &&relation, optional<string> baseRelation = nullopt);

    AssumptionType type;
    Relation relation;
    optional<string> baseRelation; // is set iff regular
};
