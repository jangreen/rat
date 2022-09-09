#pragma once
#include <unordered_map>
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
    static unordered_map<string, Relation> relations;

public:
    Relation(const string &name = "", const Operator &op = Operator::none, Relation *left = nullptr, Relation *right = nullptr);
    ~Relation();

    string name;
    Operator op;
    Relation *left;
    Relation *right;

    static Relation get(const string &name);
    static void add(const string &name, const Relation &relation);
};
