#include "Constraint.h"

Constraint::Constraint(const ConstraintType type, const Relation &&relation, const std::optional<std::string> name) : type(type), relation(std::move(relation)), name(name) {}

void Constraint::toEmptyNormalForm()
{
    if (type == ConstraintType::irreflexive)
    {
        Relation id(Operation::identity);
        relation = Relation(Operation::intersection, std::move(relation), std::move(id));
    }
    else if (type == ConstraintType::acyclic)
    {
        Relation tc(Operation::transitiveClosure, std::move(relation));
        Relation id(Operation::identity);
        relation = Relation(Operation::intersection, std::move(tc), std::move(id));
    }
    type = ConstraintType::empty;
}