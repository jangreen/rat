#pragma once
#include <string>
#include <memory>
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
    Constraint(const ConstraintType &type, shared_ptr<Relation> relation, const string &name = "");
    ~Constraint();

    string name;
    ConstraintType type;
    shared_ptr<Relation> relation;

    void toEmptyNormalForm();
};

typedef unordered_map<string, Constraint> ConstraintSet;
