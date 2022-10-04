#pragma once
#include <unordered_set>
#include "Relation.h"

using namespace std;

class ProofObligation 
{
public:
    ProofObligation();
    ~ProofObligation();

    RelationSet left;
    RelationSet right;
};


