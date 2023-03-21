#include "Relation.h"
#include "CatInferVisitor.h"

Relation::Relation(const Relation &other) : operation(other.operation), identifier(other.identifier), label(other.label), negated(other.negated), saturated(other.saturated), saturatedId(other.saturatedId)
{
    if (other.leftOperand != nullptr)
    {
        leftOperand = std::make_unique<Relation>(*other.leftOperand);
    }
    if (other.rightOperand != nullptr)
    {
        rightOperand = std::make_unique<Relation>(*other.rightOperand);
    }
}
Relation &Relation::operator=(const Relation &other)
{
    Relation copy(other);
    swap(*this, copy);
    return *this;
}

Relation::Relation(const std::string &expression)
{
    CatInferVisitor visitor;
    *this = visitor.parseRelation(expression);
}
Relation::Relation(const Operation operation, const std::optional<std::string> &identifier) : operation(operation), identifier(identifier), leftOperand(nullptr), rightOperand(nullptr) {}
Relation::Relation(const Operation operation, Relation &&left) : operation(operation), identifier(std::nullopt), rightOperand(nullptr)
{
    leftOperand = std::make_unique<Relation>(std::move(left));
}
Relation::Relation(const Operation operation, Relation &&left, Relation &&right) : operation(operation), identifier(std::nullopt)
{
    leftOperand = std::make_unique<Relation>(std::move(left));
    rightOperand = std::make_unique<Relation>(std::move(right));
}

std::unordered_map<std::string, Relation> Relation::relations;
int Relation::maxLabel = 0;

bool Relation::operator==(const Relation &otherRelation) const
{
    if (operation != otherRelation.operation)
    {
        return false;
    }

    /* modulo negation if (negated != otherRelation.negated)
    {
        return false;
    }*/

    if ((label.has_value() != otherRelation.label.has_value()) || (label && *label != *otherRelation.label))
    {
        return false;
    }

    if (operation == Operation::base)
    {
        return *identifier == *otherRelation.identifier;
    }

    if (operation == Operation::none || operation == Operation::identity || operation == Operation::empty || operation == Operation::full)
    {
        return true;
    }

    bool leftEqual = *leftOperand == *otherRelation.leftOperand;
    bool rightNullEqual = (rightOperand == nullptr) == (otherRelation.rightOperand == nullptr);
    bool rightEqual = rightNullEqual && (rightOperand == nullptr || *rightOperand == *otherRelation.rightOperand);
    return leftEqual && rightEqual;
}
bool Relation::operator<(const Relation &otherRelation) const
{
    return (toString() < otherRelation.toString());
}

bool Relation::isNormal() const
{
    if (label && operation != Operation::base && operation != Operation::none)
    {
        return false;
    }
    else if (leftOperand != nullptr) // has children
    {
        bool leftIsNormal = leftOperand == nullptr || leftOperand->isNormal();
        bool rightIsNormal = rightOperand == nullptr || rightOperand->isNormal();
        bool leftRightSpecialCase = leftOperand == nullptr || rightOperand == nullptr || leftOperand->operation != Operation::none || rightOperand->operation != Operation::none;
        return leftIsNormal && rightIsNormal && leftRightSpecialCase;
    }
    return true;
}

std::vector<int> Relation::labels() const
{
    if (label)
    {
        return {*label};
    }
    else if (leftOperand == nullptr && rightOperand == nullptr)
    {
        return {};
    }

    auto result = leftOperand->labels();
    if (rightOperand != nullptr)
    {
        auto right = rightOperand->labels();
        result.insert(result.end(), right.begin(), right.end());
    }
    return result;
}

std::vector<int> Relation::calculateRenaming() const
{
    return labels(); // labels already calculates the renaming
}

void Relation::rename(const std::vector<int> &renaming)
{
    if (label)
    {
        label = distance(renaming.begin(), find(renaming.begin(), renaming.end(), *label));
    }
    else if (leftOperand)
    {
        (*leftOperand).rename(renaming);
        if (rightOperand)
        {
            (*rightOperand).rename(renaming);
        }
    }
}

std::string Relation::toString() const
{
    std::string output;
    if (negated)
    {
        output += "-";
    }
    if (label)
    {
        output += "[" + std::to_string(*label) + "]";
    }
    switch (operation)
    {
    case Operation::intersection:
        output += "(" + leftOperand->toString() + " & " + rightOperand->toString() + ")";
        break;
    case Operation::composition:
        output += "(" + leftOperand->toString() + ";" + rightOperand->toString() + ")";
        break;
    case Operation::choice:
        output += "(" + leftOperand->toString() + " | " + rightOperand->toString() + ")";
        break;
    case Operation::converse:
        output += leftOperand->toString() + "^-1";
        break;
    case Operation::transitiveClosure:
        output += leftOperand->toString() + "^*";
        break;
    case Operation::base:
        output += *identifier;
        break;
    case Operation::identity:
        output += "id";
        break;
    case Operation::empty:
        output += "0";
        break;
    case Operation::full:
        output += "1";
        break;
    case Operation::none:
        break;
    }
    if (saturated)
    {
        output += ".";
    }
    if (saturatedId)
    {
        output += "..";
    }
    return output;
}