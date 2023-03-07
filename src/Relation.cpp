#include <iostream>
#include "Relation.h"
#include "CatInferVisitor.h"

using namespace std;

Relation::Relation(const Operation operation, const optional<string> &identifier) : operation(operation), identifier(identifier), leftOperand(nullptr), rightOperand(nullptr) {}
Relation::Relation(const Operation operation, const shared_ptr<Relation> left) : operation(operation), identifier(nullopt), leftOperand(left), rightOperand(nullptr) {}
Relation::Relation(const Operation operation, const shared_ptr<Relation> left, const shared_ptr<Relation> right) : operation(operation), identifier(nullopt), leftOperand(left), rightOperand(right) {}

unordered_map<string, shared_ptr<Relation>> Relation::relations;
int Relation::maxLabel = 0;

shared_ptr<Relation> Relation::parse(const string &expression)
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
        output += "-.";
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
    return output;
}