#include "Assumption.h"

#include <utility>

// helper
void Assumption::markBaseRelationsAsSaturated(Relation &relation, int saturatedCount) {
  if (relation.operation == RelationOperation::base) {
    relation.saturated = saturatedCount;
  } else {
    if (relation.leftOperand != nullptr) {
      markBaseRelationsAsSaturated(*relation.leftOperand, saturatedCount);
    }
    if (relation.rightOperand != nullptr) {
      markBaseRelationsAsSaturated(*relation.rightOperand, saturatedCount);
    }
  }
}

Assumption::Assumption(const AssumptionType type, Relation &&relation,
                       std::optional<std::string> baseRelation)
    : type(type), relation(std::move(relation)), baseRelation(baseRelation) {
  if (this->type != AssumptionType::empty) {
    markBaseRelationsAsSaturated(this->relation, 1);
  }
}
