#pragma once
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <string>

using namespace std;

enum class Operation
{
    none, // base relation, or empty relation (only label)
    choice,
    intersection,
    composition,
    transitiveClosure,
    converse
};

class Relation
{
public:
    Relation(const optional<string> &identifier); // base relation constructor, do not use directly -> use get method
    Relation(const Operation &operation, shared_ptr<Relation> left = nullptr, shared_ptr<Relation> right = nullptr, bool negated = false, optional<int> label = nullopt);
    ~Relation();

    Operation operation;
    optional<string> identifier;       // is set iff operation none
    shared_ptr<Relation> leftOperand;  // is set iff operation unary/binary
    shared_ptr<Relation> rightOperand; // is set iff operation binary
    bool negated;
    optional<int> label;

    bool isNormal(); // true iff all labels are in front of base relations

    static shared_ptr<Relation> ID;                               // constant: idendtity relation
    static shared_ptr<Relation> EMPTY;                            // constant: empty relation
    static shared_ptr<Relation> FULL;                             // constant: full relation
    static unordered_map<string, shared_ptr<Relation>> relations; // id, 0, 1, base relations and defined relations (named relations)
    static int maxLabel;                                          // to create globally unique labels
    static shared_ptr<Relation> get(const string &identifier);
    static shared_ptr<Relation> parse(const string &expression);

    bool operator==(const Relation &other) const; // compares two relation syntactically

    string toString() const; // for printing
};

typedef unordered_set<shared_ptr<Relation>> RelationSet; // TODO: refactor use hashing? not needed for axiom but maybe for proofNodes