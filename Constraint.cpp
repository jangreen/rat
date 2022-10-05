#include "Constraint.h"
#include "Relation.h"
#include <iostream>

using namespace std;

Constraint::Constraint() {}
Constraint::Constraint(const ConstraintType &type, shared_ptr<Relation> relation, const string &name) : type(type), relation(relation), name(name) {}
Constraint::~Constraint() {}

void Constraint::toEmptyNormalForm()
{
    if (type == ConstraintType::irreflexive)
    {
        relation = make_shared<Relation>(Operator::cap, relation, Relation::ID);
    }
    else if (type == ConstraintType::acyclic)
    {
        shared_ptr<Relation> tc = make_shared<Relation>(Operator::transitive, relation);
        relation = make_shared<Relation>(Operator::cap, tc, Relation::ID);
    }
}