#include <iostream>
#include "Relation.h"

using namespace std;

Relation::Relation(const string &name, const Operator &op, Relation *left, Relation *right) : name(name), op(op), left(left), right(right) {}
Relation::~Relation() {}
unordered_map<string, Relation> Relation::relations = {}; // TODO needed?

Relation Relation::get(const string &name)
{
    return relations[name];
}

void Relation::add(const string &name, const Relation &relation)
{
    relations[name] = relation;
}