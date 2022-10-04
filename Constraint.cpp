#include "Constraint.h"
#include "Relation.h"
#include <iostream>

using namespace std;

Constraint::Constraint() {}
Constraint::Constraint(const ConstraintType &type, Relation *relation, const string &name) : type(type), relation(relation), name(name) {}
Constraint::~Constraint() {}

Constraint Constraint::emptyNormalForm() {
    if (type == ConstraintType::empty) {
        return *this;
    }
    else if (type == ConstraintType::irreflexive)
    {
        Relation r = Relation("", Operator::cap, relation, Relation::id);
        relation = &r;
        return *this;
    }
    else if (type == ConstraintType::acyclic) {
        Relation tc = Relation("", Operator::transitive, relation);
        Relation r = Relation("", Operator::cap, &tc, Relation::id);
        relation = &r;
        return *this;
    }
    return *this;
}