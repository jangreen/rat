#include "Constraint.h"

#include <utility>

Constraint::Constraint(const ConstraintType type, const Relation &&relation,
                       const std::optional<std::string> name)
    : type(type), relation(std::move(relation)), name(name) {}

void Constraint::toEmptyNormalForm() {
  if (type == ConstraintType::irreflexive) {
    Relation id(RelationOperation::identity);
    relation = Relation(RelationOperation::intersection, std::move(relation), std::move(id));
  } else if (type == ConstraintType::acyclic) {
    Relation id(RelationOperation::identity);
    Relation copyR(relation);
    Relation reflTc(RelationOperation::transitiveClosure, std::move(relation));
    Relation tc(RelationOperation::composition, std::move(copyR), std::move(reflTc));
    relation = Relation(RelationOperation::intersection, std::move(tc), std::move(id));
  }
  type = ConstraintType::empty;
}
