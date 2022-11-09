#pragma once
#include "ProofNode.h"

using namespace std;

class RegSolver
{
public:
    RegSolver();
    ~RegSolver();

    shared_ptr<ProofNode> goal;
    bool solve();
};
