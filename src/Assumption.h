#pragma once
#include <string>
#include <memory>
#include "Relation.h"

using namespace std;

enum class AssumptionType
{
    regular,
    empty,   // TODO
    identity // TODO
};

class Assumption
{
public:
    Assumption(const AssumptionType type, Relation &&relation, optional<string> baseRelation = nullopt);

    AssumptionType type;
    unique_ptr<Relation> relation;
    optional<string> baseRelation; // is set iff regular
};
