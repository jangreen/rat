#include "Constraint.h"

Constraint::Constraint(const ConstraintType type, const Relation &&relation, const optional<string> name) : type(type), relation(std::move(relation)), name(name) {}

void Constraint::toEmptyNormalForm()
{
    if (type == ConstraintType::irreflexive)
    {
        Relation id = Relation(Operation::identity);
        relation = Relation(Operation::intersection, std::move(relation), std::move(id));
    }
    else if (type == ConstraintType::acyclic)
    {
        Relation tc = Relation(Operation::transitiveClosure, std::move(relation));
        Relation id = Relation(Operation::identity);
        relation = Relation(Operation::intersection, std::move(tc), std::move(id));
    }
    type = ConstraintType::empty;
}