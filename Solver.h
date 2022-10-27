#pragma once
#include <stack>
#include <set>
#include "ProofNode.h"

using namespace std;

class Solver
{
public:
    Solver();
    ~Solver();

    stack<shared_ptr<ProofNode>> goals; // goals on stack that are not closed yet
    Theory theory;                      // inequalities that are true
    set<shared_ptr<ProofNode>> unprovable;
    set<shared_ptr<ProofNode>> proved;
    bool stepwise;
    bool silent;

    void load(string model1, string model2);
    bool solve(); // models alread< loaded
    bool solve(string model1, string model2);

    bool isCycle(shared_ptr<ProofNode> node);
    shared_ptr<ProofNode> childProofNode(shared_ptr<ProofNode> node);

    bool axiomEmpty(shared_ptr<ProofNode> node);
    bool axiomFull(shared_ptr<ProofNode> node);
    bool axiomEqual(shared_ptr<ProofNode> node);
    bool axiomTheory(shared_ptr<ProofNode> node);
    bool andLeftRule(shared_ptr<ProofNode> node);
    bool andRightRule(shared_ptr<ProofNode> node);
    bool orLeftRule(shared_ptr<ProofNode> node);
    bool orRightRule(shared_ptr<ProofNode> node);
    bool inverseRule(shared_ptr<ProofNode> node);
    bool seqLeftRule(shared_ptr<ProofNode> node);
    bool simplifyTcRule(shared_ptr<ProofNode> node);
    bool transitiveClosureRule(shared_ptr<ProofNode> node);
    bool unrollRule(shared_ptr<ProofNode> node); // TODO: bounded
    bool cutRule(shared_ptr<ProofNode> node);
    bool consRule(shared_ptr<ProofNode> node);
    bool weakRightRule(shared_ptr<ProofNode> node);
    bool weakLeftRule(shared_ptr<ProofNode> node);
    bool loopRule(shared_ptr<ProofNode> node);

    bool invcapEmptyRule(shared_ptr<ProofNode> node);
    bool idseqEmptyRule(shared_ptr<ProofNode> node);

    string toDotFormat(shared_ptr<ProofNode> node);
    void exportProof(shared_ptr<ProofNode> root);
};

// TODO remove: typedef void (*ProofRule)(ProofNode &);