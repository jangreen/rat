#pragma once
#include <unordered_set>
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
private:
    static unordered_set<shared_ptr<Relation>> relations = { EMPTY, FULL, ID };

public:
    Relation(const Operator &op = Operator::none, Relation *left = nullptr, Relation *right = nullptr);
    ~Relation();

    Operator op;
    shared_ptr<Relation> left; // set if operator unary/binary
    shared_ptr<Relation> right; // set if operator binary

    string description();

    static const Relation ID;
    static const Relation EMPTY;
    static const Relation FULL;

    static Relation get(const string &name);
    static bool defined(const string &name);
    static void add(const string &name, const Relation &relation);
    static Relation *id;
    static Relation *empty;
    static Relation *full;

    bool operator==(const Relation &other) const;

    struct HashFunction {
        size_t operator()(const Relation &relation) const;
    };
};

typedef unordered_set<Relation, Relation::HashFunction> RelationSet;