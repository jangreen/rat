#include <iostream>
#include "Relation.h"
#include "CatInferVisitor.h"

using namespace std;

Relation::Relation(const optional<string> &identifier) : operation(Operation::none), identifier(identifier), leftOperand(nullptr), rightOperand(nullptr), label(nullopt) {}
Relation::Relation(const Operation &operation, shared_ptr<Relation> left, shared_ptr<Relation> right, bool negated, optional<int> label) : operation(operation), identifier(""), leftOperand(left), rightOperand(right), negated(negated), label(label) {}
Relation::~Relation() {}

shared_ptr<Relation> Relation::ID = make_shared<Relation>("id");
shared_ptr<Relation> Relation::EMPTY = make_shared<Relation>("0");
shared_ptr<Relation> Relation::FULL = make_shared<Relation>("1");
unordered_map<string, shared_ptr<Relation>> Relation::relations = {{"0", Relation::EMPTY}, {"1", Relation::FULL}, {"id", Relation::ID}};
int Relation::maxLabel = 0;

shared_ptr<Relation> Relation::get(const string &name)
{
    if (!relations.contains(name))
    {
        relations[name] = make_shared<Relation>(name);
    }
    return relations[name];
}

shared_ptr<Relation> Relation::parse(const string &expression)
{
    CatInferVisitor visitor;
    return visitor.parseRelation(expression);
}

bool Relation::operator==(const Relation &other) const
{
    if (operation != other.operation || label != other.label)
    {
        return false;
    }
    else if (operation == Operation::none)
    {
        return !identifier || (this == &other);
    }
    else
    {
        return *leftOperand == *other.leftOperand && ((rightOperand == nullptr && other.rightOperand == nullptr) || *rightOperand == *other.rightOperand);
    }
}

string Relation::toString() const
{
    string output;
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
    case Operation::none:
        if (identifier)
        {
            output += *identifier;
        }
        break;
    }
    return output;
}