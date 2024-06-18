#include "Assumption.h"

#include <utility>

// helper
void Assumption::markBaseRelationsAsSaturated(Relation &relation, int saturatedCount, bool base) {
  if (relation.operation == RelationOperation::base) {
    if (base) {
      relation.saturatedBase = saturatedCount;
    } else {
      relation.saturatedId = saturatedCount;
    }
  } else {
    if (relation.leftOperand != nullptr) {
      markBaseRelationsAsSaturated(*relation.leftOperand, saturatedCount, base);
    }
    if (relation.rightOperand != nullptr) {
      markBaseRelationsAsSaturated(*relation.rightOperand, saturatedCount, base);
    }
  }
}

Assumption::Assumption(const AssumptionType type, Relation &&relation,
                       std::optional<std::string> baseRelation)
    : type(type), relation(std::move(relation)), baseRelation(baseRelation) {
  switch (this->type) {
    case AssumptionType::empty:
      break;
    case AssumptionType::regular:
      markBaseRelationsAsSaturated(this->relation, 1, true);
      break;
    case AssumptionType::identity:
      markBaseRelationsAsSaturated(this->relation, 1, false);
      break;
  }
}
