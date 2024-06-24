#include "Constraint.h"

Constraint::Constraint(const ConstraintType type, const CanonicalRelation relation,
                       const std::optional<std::string>& name)
    : type(type), relation(relation), name(name) {}

// void Constraint::toEmptyNormalForm() {
//   if (type == ConstraintType::irreflexive) {
//     CanonicalRelation id = Relation::newRelation(RelationOperation::identity);
//     relation = Relation(RelationOperation::intersection, relation, id);
//   } else if (type == ConstraintType::acyclic) {
//     CanonicalRelation id(RelationOperation::identity);
//     CanonicalRelation copyR(relation);
//     CanonicalRelation reflTc(RelationOperation::transitiveClosure, relation);
//     CanonicalRelation tc(RelationOperation::composition, copyR, reflTc);
//     relation = Relation(RelationOperation::intersection, tc, id);
//   }
//   type = ConstraintType::empty;
// }
