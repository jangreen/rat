#include "Constraint.h"

#include <utility>

Constraint::Constraint(const ConstraintType type, const Relation &&relation,
                       const std::optional<std::string> name)
    : type(type), relation(std::move(relation)), name(name) {}

void Constraint::toEmptyNormalForm() {
  if (type == ConstraintType::irreflexive) {
    relation = Relation(Operation::intersection, std::move(relation), Operation::identity);
  } else if (type == ConstraintType::acyclic) {
    Relation tc(Operation::transitiveClosure, std::move(relation));
    relation = Relation(Operation::intersection, std::move(tc), Operation::identity);
  }
  type = ConstraintType::empty;
}
