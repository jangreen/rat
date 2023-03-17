#include "Assumption.h"

using namespace std;

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

Assumption::Assumption(const AssumptionType type, Relation &&relation, optional<string> baseRelation) : type(type), relation(std::move(relation)), baseRelation(baseRelation)
{
    markBaseAsSaturated(this->relation);
}
