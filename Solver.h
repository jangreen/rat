#pragma once
#include "ProofNode.h"
#include <stack>
#include <memory>

using namespace std;

class Solver
{
public:
    Solver();
    ~Solver();

    stack<shared_ptr<ProofNode>> goals; // goals on stack that are not closed yet
    Theory theory;                         // inequalities that are true

    void solve();

    string toDotFormat(shared_ptr<ProofNode> node);

    bool axiom(shared_ptr<ProofNode> node);
    bool andLeftRule(shared_ptr<ProofNode> node);
    bool andRightRule(shared_ptr<ProofNode> node);
    bool orLeftRule(shared_ptr<ProofNode> node);
    bool orRightRule(shared_ptr<ProofNode> node);
    bool transitiveClosureRule(shared_ptr<ProofNode> node);

    bool prove(ProofNode goal);
};

// TODO remove: typedef void (*ProofRule)(ProofNode &);