#pragma once
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <string>

using namespace std;

enum class Operator
{
    none, // base relation
    cup,
    cap,
    composition,
    transitive,
    inverse,
    complement, // TODO: not supported
    setminus    // TODO: not supported
};

class Relation
{
public:
    Relation(const string &identifier); // base relation constructor, do not use directly -> use get method
    Relation(const Operator &op = Operator::none, shared_ptr<Relation> left = nullptr, shared_ptr<Relation> right = nullptr);
    ~Relation();

    Operator op;
    string identifier;          // optional: set iff operator none
    shared_ptr<Relation> left;  // optional: set iff operator unary/binary
    shared_ptr<Relation> right; // optional: set iff operator binary

    static shared_ptr<Relation> ID;
    static shared_ptr<Relation> EMPTY;
    static shared_ptr<Relation> FULL;
    static unordered_map<string, shared_ptr<Relation>> relations; // id, 0, 1, base relations and defined relations (named relations)
    static shared_ptr<Relation> get(const string &identifier);
    static shared_ptr<Relation> parse(const string &expression);

    bool operator==(const Relation &other) const; // compares two relation syntactically // TODO: needed or remove?

    string toString(); // for printing
};

typedef unordered_set<shared_ptr<Relation>> RelationSet; // TODO: refactor use hashing? not needed for axiom but maybe for proofNodes