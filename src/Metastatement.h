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
    Metastatement();
    Metastatement(const MetastatementType &type, int label1, int label2, optional<string> baseRelation = nullopt);
    ~Metastatement();

    MetastatementType type;
    int label1;
    int label2;
    optional<string> baseRelation; // is set iff labelRelation

    string toString(); // for printing
};
