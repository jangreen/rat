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
    Theory theory;                      // inequalities that are true

    void load(string model1, string model2);
    bool solve(); // models alread< loaded
    bool solve(string model1, string model2);

    bool axiom(shared_ptr<ProofNode> node);
    bool andLeftRule(shared_ptr<ProofNode> node);
    bool andRightRule(shared_ptr<ProofNode> node);
    bool orLeftRule(shared_ptr<ProofNode> node);
    bool orRightRule(shared_ptr<ProofNode> node);
    bool inverseRule(shared_ptr<ProofNode> node);
    bool seqLeftRule(shared_ptr<ProofNode> node);
    bool transitiveClosureRule(shared_ptr<ProofNode> node);
    bool unrollRule(shared_ptr<ProofNode> node); // TODO: bounded
    bool cutRule(shared_ptr<ProofNode> node);
    bool consRule(shared_ptr<ProofNode> node);
    bool loopRule(shared_ptr<ProofNode> node);

    string toDotFormat(shared_ptr<ProofNode> node);
};

// TODO remove: typedef void (*ProofRule)(ProofNode &);