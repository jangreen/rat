#pragma once
#include <string>
#include "Relation.h"

using namespace std;

enum class ConstraintType {
    empty,
    irreflexive,
    acyclic
};

class Constraint
{
public:
    Constraint();
    Constraint(const ConstraintType &type, Relation *relation, const string &name = "");
    ~Constraint();

    string name;
    ConstraintType type;
    Relation *relation;

    Constraint emptyNormalForm();
};

typedef unordered_map<string, Constraint> ConstraintSet;
