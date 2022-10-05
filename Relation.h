#pragma once
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <string>

using namespace std;

enum class Operator
{
    none,
    cup,
    cap,
    setminus,
    composition,
    transitive,
    inverse,
    complement
};

class Relation
{
public:
    Relation(const string &alias);
    Relation(const Operator &op = Operator::none, shared_ptr<Relation> left = nullptr, shared_ptr<Relation> right = nullptr);
    ~Relation();

    Operator op;
    string alias;               // set if operator none
    shared_ptr<Relation> left;  // set if operator unary/binary
    shared_ptr<Relation> right; // set if operator binary

    string toString();

    static shared_ptr<Relation> ID;
    static shared_ptr<Relation> EMPTY;
    static shared_ptr<Relation> FULL;
    static unordered_map<string, shared_ptr<Relation>> relations;
    static shared_ptr<Relation> get(const string &name);

    bool operator==(const Relation &other) const;
};

typedef unordered_set<shared_ptr<Relation>> RelationSet;