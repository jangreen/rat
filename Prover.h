#pragma once
#include "ProofObligation.h"

using namespace std;

class Prover
{
public:
    Prover();
    ~Prover();

    bool prove(ProofObligation goal);
};
