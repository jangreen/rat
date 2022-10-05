#include <iostream>
#include "Relation.h"

using namespace std;

Relation::Relation(const string &alias) : alias(alias), op(Operator::none), left(nullptr), right(nullptr) {}
Relation::Relation(const Operator &op, shared_ptr<Relation> left, shared_ptr<Relation> right) : alias("-"), op(op), left(left), right(right) {}
Relation::~Relation() {}
unordered_map<string, shared_ptr<Relation>> Relation::relations = {{"0", Relation::EMPTY}, {"1", Relation::FULL}, {"id", Relation::ID}};
shared_ptr<Relation> Relation::ID = make_shared<Relation>("id");
shared_ptr<Relation> Relation::EMPTY = make_shared<Relation>("0");
shared_ptr<Relation> Relation::FULL = make_shared<Relation>("1");

shared_ptr<Relation> Relation::get(const string &name)
{
    if (!relations.contains(name)) {
        shared_ptr<Relation> baseRelation = make_shared<Relation>(name);
        relations[name] = baseRelation;
    }
    return relations[name];
}

string Relation::toString()
{
    switch (op)
    {
    case Operator::cap:
        return "(" + left->toString()+ " & " + right->toString() + ")";
    case Operator::complement:
        return "-" + left->toString();
    case Operator::composition:
        return "(" + left->toString() + ";" + right->toString() + ")";
    case Operator::cup:
        return "(" + left->toString() + " | " + right->toString() + ")";
    case Operator::inverse:
        return left->toString() + "^-1";
    case Operator::setminus:
        return "(" + left->toString() + "\\" + right->toString() + ")";
    case Operator::transitive:
        return left->toString() + "^+";
    case Operator::none:
        return alias;
    default:
        return "-";
    }
}

// compares two relation syntactically
bool Relation::operator==(const Relation &other) const
{
    if (op != other.op)
    {
        return false;
    }
    else if (op == Operator::none)
    {
        return alias == other.alias;
    }
    else
    {
        return left == other.left && right == other.right;
    }
}

