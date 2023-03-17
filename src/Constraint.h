#pragma once
#include <string>
#include <memory>
#include "Relation.h"

using namespace std;

enum class ConstraintType
{
    empty,
    irreflexive,
    acyclic
};

class Constraint
{
public:
    Constraint(const ConstraintType type, const Relation &&relation, const optional<string> name = nullopt);

    ConstraintType type;
    Relation relation;
    optional<string> name; // for printing

    void toEmptyNormalForm();
};
