#include <iostream>
#include "Relation.h"

using namespace std;

Relation::Relation(const string &alias, const Operator &op, Relation *left, Relation *right) : name(name), op(op), left(left), right(right) {}
Relation::~Relation() {}
// TODO remove: unordered_map<string, unique_ptr<Relation>> Relation::relations =  // TODO needed?
//Relation *Relation::id = &relations["id"];
//Relation *Relation::empty = &relations["0"];
//Relation *Relation::full = &relations["1"];

Relation Relation::get(const string &name)
{
    return relations[name];
}

bool Relation::defined(const string &name)
{
    return relations.contains(name);
}

string Relation::description()
{
    switch (op)
    {
    case Operator::cap:
        std::cout << "cap" << std::endl;

        return "(" + left->description()+ " & " + right->description() + ")";
    case Operator::complement:
        std::cout << "comp" << std::endl;

        return "-" + left->description();
    case Operator::composition:
        std::cout << "composition" << std::endl;

        return "(" + left->description() + ";" + right->description() + ")";
    case Operator::cup:
        std::cout << "cup" << std::endl;

        return "(" + left->description() + " | " + right->description() + ")";
    case Operator::inverse:
        std::cout << "inv" << std::endl;

        return left->description() + "^-1";
    case Operator::setminus:
        std::cout << "setmin" << std::endl;

        return "(" + left->description() + "\\" + right->description() + ")";
    case Operator::transitive:
        std::cout << "tc" << std::endl;

        return left->description() + "^+";
    case Operator::none:
        std::cout << "none" << std::endl;

        return name;
    default:
        std::cout << "def" << std::endl;
        std::cout << name << std::endl;
        std::cout << left << std::endl;

        if (left != nullptr) {
            //return left->description();
        }
        return "";
    }
}

void Relation::add(const string &name, const Relation &relation)
{
    relations[name] = relation;
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
        return name == other.name;
    }
    else
    {
        return left == other.left && right == other.right;
    }
}

size_t Relation::HashFunction::operator()(const Relation &relation) const
{
    if (relation.op == Operator::none){
        return hash<string>()(relation.name);
    }
    else
    {
        // Compute individual hash values for first,
        // second and third and combine them using XOR
        // and bit shifting:
        size_t leftHash = HashFunction::operator()(*relation.left);
        size_t rightHash = HashFunction::operator()(*relation.right);
        size_t opHash = static_cast<std::size_t>(relation.op);
        return (leftHash ^ (rightHash << 1) >> 1) ^ (opHash << 1);
    }
};
