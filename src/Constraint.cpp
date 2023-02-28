#include "Constraint.h"
#include "Relation.h"

using namespace std;

Constraint::Constraint() {}
Constraint::Constraint(const ConstraintType &type, shared_ptr<Relation> relation, const string &name) : type(type), relation(relation), name(name) {}
Constraint::~Constraint() {}

void Constraint::toEmptyNormalForm()
{
    if (type == ConstraintType::irreflexive)
    {
        relation = make_shared<Relation>(Operation::intersection, relation, Relation::ID);
    }
    else if (type == ConstraintType::acyclic)
    {
        shared_ptr<Relation> tc = make_shared<Relation>(Operation::transitiveClosure, relation);
        relation = make_shared<Relation>(Operation::intersection, tc, Relation::ID);
    }
    type = ConstraintType::empty;
}