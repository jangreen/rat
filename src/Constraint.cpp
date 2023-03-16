#include "Constraint.h"

Constraint::Constraint(const Constraint &other) : type(other.type), name(other.name)
{
    relation = make_unique<Relation>(*other.relation);
}
Constraint::Constraint(const ConstraintType type, const Relation &&relation, const optional<string> name) : type(type), relation(make_unique<Relation>(relation)), name(name) {}

void Constraint::toEmptyNormalForm()
{
    if (type == ConstraintType::irreflexive)
    {
        Relation id = Relation(Operation::identity);
        relation = make_unique<Relation>(Operation::intersection, move(*relation), move(id));
    }
    else if (type == ConstraintType::acyclic)
    {
        Relation tc = Relation(Operation::transitiveClosure, move(*relation));
        Relation id = Relation(Operation::identity);
        relation = make_unique<Relation>(Operation::intersection, move(tc), move(id));
    }
    type = ConstraintType::empty;
}