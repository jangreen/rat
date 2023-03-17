#include "Assumption.h"

// helper
void markBaseAsSaturated(Relation &relation)
{
    if (relation.operation == Operation::base)
    {
        relation.saturated = true;
    }
    else
    {
        if (relation.leftOperand != nullptr)
        {
            markBaseAsSaturated(*relation.leftOperand);
        }
        if (relation.rightOperand != nullptr)
        {
            markBaseAsSaturated(*relation.rightOperand);
        }
    }
}

Assumption::Assumption(const AssumptionType type, Relation &&relation, std::optional<std::string> baseRelation) : type(type), relation(std::move(relation)), baseRelation(baseRelation)
{
    markBaseAsSaturated(this->relation);
}
