#include "Constraint.h"

Constraint::Constraint(const ConstraintType type, const shared_ptr<Relation> relation, const optional<string> name) : type(type), relation(relation), name(name) {}

void Constraint::toEmptyNormalForm()
{
    if (type == ConstraintType::irreflexive)
    {
        shared_ptr<Relation> id = make_shared<Relation>(Operation::identity);
        relation = make_shared<Relation>(Operation::intersection, relation, id);
    }
    else if (type == ConstraintType::acyclic)
    {
        shared_ptr<Relation> tc = make_shared<Relation>(Operation::transitiveClosure, relation);
        shared_ptr<Relation> id = make_shared<Relation>(Operation::identity);
        relation = make_shared<Relation>(Operation::intersection, tc, id);
    }
    type = ConstraintType::empty;
}