#pragma once
#include <string>
#include <optional>

using namespace std;

enum class MetastatementType
{
    labelEquality,
    labelRelation
};

class Metastatement
{
public:
    Metastatement(const MetastatementType type, int label1, int label2, optional<string> baseRelation = nullopt);

    MetastatementType type;
    int label1;
    int label2;
    optional<string> baseRelation; // is set iff labelRelation

    string toString() const; // for printing
};
