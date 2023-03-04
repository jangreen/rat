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
    Constraint(const ConstraintType type, const shared_ptr<Relation> relation, const optional<string> name = nullopt);

    ConstraintType type;
    shared_ptr<Relation> relation;
    optional<string> name; // for printing

    void toEmptyNormalForm();
};
