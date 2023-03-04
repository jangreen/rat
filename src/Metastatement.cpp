#include "Metastatement.h"

using namespace std;

Metastatement::Metastatement(const MetastatementType type, int label1, int label2, optional<string> baseRelation) : type(type), label1(label1), label2(label2), baseRelation(baseRelation) {}

string Metastatement::toString() const
{
    string output;
    if (type == MetastatementType::labelRelation)
    {
        output = "(" + to_string(label1) + "," + to_string(label2) + ") -> " + *baseRelation;
    }
    else
    {
        output = "TODO";
    }
    return output;
}