#pragma once
#include <unordered_map>
#include <memory>
#include <string>

using namespace std;

enum class Operation
{
    none,              // no relation, only label
    base,              // nullary function (aka constant): base relation
    identity,          // nullary function (aka constant): identity relation
    empty,             // nullary function (aka constant): empty relation
    choice,            // binary function
    intersection,      // binary function
    composition,       // binary function
    transitiveClosure, // unary function
    converse           // unary function
};

class Relation
{
public:
    Relation(const Operation operation, const optional<string> &identifier = nullopt);                      // nullary
    Relation(const Operation operation, const shared_ptr<Relation> left);                                   // unary
    Relation(const Operation operation, const shared_ptr<Relation> left, const shared_ptr<Relation> right); // binary

    Operation operation;
    optional<string> identifier;       // is set iff operation base
    shared_ptr<Relation> leftOperand;  // is set iff operation unary/binary
    shared_ptr<Relation> rightOperand; // is set iff operation binary
    optional<int> label = nullopt;
    bool negated = false;
    bool saturated = false;

    bool isNormal() const;                 // true iff all labels are in front of base relations
    vector<int> labels() const;            // return all labels of the relation term
    vector<int> calculateRenaming() const; // renaming {2,4,5}: 2->0,4->1,5->2
    void rename(const vector<int> &renaming);
    bool operator==(const Relation &otherRelation) const; // compares two relation syntactically
    string toString() const;                              // for printing

    static unordered_map<string, shared_ptr<Relation>> relations; // id, 0, 1, base relations and defined relations (named relations)
    static int maxLabel;                                          // to create globally unique labels
    static shared_ptr<Relation> parse(const string &expression);
};
