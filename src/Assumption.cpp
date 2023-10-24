#include "Assumption.h"

#include <utility>

// helper
void markBaseRelationsAsSaturated(bool identity, Relation &relation) {
  if (relation.operation == RelationOperation::base) {
    if (identity) {
      relation.saturatedId = true;
    } else {
      relation.saturated = true;
    }
  } else {
    if (relation.leftOperand != nullptr) {
      markBaseRelationsAsSaturated(identity, *relation.leftOperand);
    }
    if (relation.rightOperand != nullptr) {
      markBaseRelationsAsSaturated(identity, *relation.rightOperand);
    }
  }
}

Assumption::Assumption(const AssumptionType type, Relation &&relation,
                       std::optional<std::string> baseRelation)
    : type(type), relation(std::move(relation)), baseRelation(baseRelation) {
  if (this->type != AssumptionType::empty) {
    markBaseRelationsAsSaturated((this->type == AssumptionType::identity), this->relation);
  }
}
