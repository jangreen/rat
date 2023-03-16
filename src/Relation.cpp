#include <iostream>
#include "Relation.h"
#include "CatInferVisitor.h"

using namespace std;

/* Rule of Five */
Relation::Relation(const Relation &other) : operation(other.operation), identifier(other.identifier), label(other.label), negated(other.negated), saturated(other.saturated)
{
    if (other.leftOperand != nullptr)
    {
        leftOperand = make_unique<Relation>(*other.leftOperand);
    }
    if (other.rightOperand != nullptr)
    {
        rightOperand = make_unique<Relation>(*other.rightOperand);
    }
}
Relation &Relation::operator=(Relation other)
{
    swap(*this, other);
    return *this;
}
Relation::Relation(Relation &&other) noexcept : Relation(Operation::none)
{
    swap(*this, other);
}

/* TODO: Relation::Relation(const string &expression) : Relation(Operation::none)
{
    CatInferVisitor visitor;
    Relation parsedRelation = visitor.parseRelation(expression);
    swap(*this, parsedRelation);
}*/
Relation::Relation(const Operation operation, const optional<string> &identifier) : operation(operation), identifier(identifier), leftOperand(nullptr), rightOperand(nullptr) {}
Relation::Relation(const Operation operation, Relation &&left) : operation(operation), identifier(nullopt), rightOperand(nullptr)
{
    leftOperand = make_unique<Relation>(move(left));
}
Relation::Relation(const Operation operation, Relation &&left, Relation &&right) : operation(operation), identifier(nullopt)
{
    leftOperand = make_unique<Relation>(move(left));
    rightOperand = make_unique<Relation>(move(right));
}

unordered_map<string, Relation> Relation::relations;
int Relation::maxLabel = 0;

Relation Relation::parse(const string &expression)
{
    CatInferVisitor visitor;
    return visitor.parseRelation(expression);
}

bool Relation::operator==(const Relation &otherRelation) const
{
    if (operation != otherRelation.operation)
    {
        return false;
    }

    if ((label.has_value() != otherRelation.label.has_value()) && (!label || *label != *otherRelation.label))
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
        return leftIsNormal && rightIsNormal;
    }
    return true;
}

vector<int> Relation::labels() const
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

vector<int> Relation::calculateRenaming() const
{
    return labels(); // labels already calculates the renaming
}

void Relation::rename(const vector<int> &renaming)
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

string Relation::toString() const
{
    string output;
    if (negated)
    {
        output += "-";
    }
    if (label)
    {
        output += "[" + to_string(*label) + "]";
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
    return output;
}