#pragma once
#include <unordered_map>
#include <memory>
#include <string>

using namespace std;

enum class Operation
{
    none,              // no relation, only label
    base,              // nullary function (constant): base relation
    identity,          // nullary function (constant): identity relation
    empty,             // nullary function (constant): empty relation
    full,              // nullary function (constant): full relation
    choice,            // binary function
    intersection,      // binary function
    composition,       // binary function
    transitiveClosure, // unary function
    converse           // unary function
};

class Relation
{
public:
    /* Rule of five */
    Relation(const Relation &other);
    Relation &operator=(Relation other);
    Relation(Relation &&other) noexcept;
    friend void swap(Relation &first, Relation &second)
    {
        using std::swap;
        swap(first.operation, second.operation);
        swap(first.identifier, second.identifier);
        swap(first.leftOperand, second.leftOperand);
        swap(first.rightOperand, second.rightOperand);
        swap(first.label, second.label);
        swap(first.negated, second.negated);
        swap(first.saturated, second.saturated);
    }

    // TODO: Relation(const string &expression);                                                // parse constructor
    Relation(const Operation operation, const optional<string> &identifier = nullopt); // nullary
    Relation(const Operation operation, Relation &&left);                              // unary
    Relation(const Operation operation, Relation &&left, Relation &&right);            // binary

    Operation operation;
    optional<string> identifier;       // is set iff operation base
    unique_ptr<Relation> leftOperand;  // is set iff operation unary/binary
    unique_ptr<Relation> rightOperand; // is set iff operation binary
    optional<int> label = nullopt;     // is set iff labeled term
    bool negated = false;              // propsitional negation
    bool saturated = false;            // mark base relation

    bool isNormal() const;                                // true iff all labels are in front of base relations
    vector<int> labels() const;                           // return all labels of the relation term
    vector<int> calculateRenaming() const;                // renaming {2,4,5}: 2->0,4->1,5->2
    void rename(const vector<int> &renaming);             // renames given a renaming function
    bool operator==(const Relation &otherRelation) const; // compares two relation syntactically
    string toString() const;                              // for printing

    static unordered_map<string, Relation> relations; // id, 0, 1, base relations and defined relations (named relations)
    static int maxLabel;                              // to create globally unique labels
    static Relation parse(const string &expression);
};

typedef vector<Relation> Clause;