#include "Assumption.h"

#include <utility>

// helper
void markBaseAsSaturated(bool identity, Relation &relation) {
  if (relation.operation == Operation::base) {
    if (identity) {
      relation.saturatedId = true;
    } else {
      relation.saturated = true;
    }
  } else {
    if (relation.leftOperand != nullptr) {
      markBaseAsSaturated(identity, *relation.leftOperand);
    }
    if (relation.rightOperand != nullptr) {
      markBaseAsSaturated(identity, *relation.rightOperand);
    }
  }
}

Assumption::Assumption(const AssumptionType type, Relation &&relation,
                       std::optional<std::string> baseRelation)
    : type(type), relation(std::move(relation)), baseRelation(baseRelation) {
  markBaseAsSaturated((this->type == AssumptionType::identity), this->relation);
}
