#pragma once
#include <stack>
#include <set>
#include <map>
#include "ProofNode.h"

using namespace std;

class Solver
{
public:
    Solver();
    ~Solver();

    stack<shared_ptr<ProofNode>> goals;                     // goals on stack that are not closed yet
    Theory theory;                                          // inequalities that are true
    static map<int, set<shared_ptr<ProofNode>>> unprovable; // relative complete: unprovable under given bounds
    static set<shared_ptr<ProofNode>> proved;               // TODO: rename: closedGoals
    bool stepwise;
    int logLevel = 0; // 0 nothing 1 only important 2 all
    static int iterations;

    void load(string model1, string model2);
    bool solve(); // models alread< loaded, main logic
    bool solve(initializer_list<shared_ptr<ProofNode>> goals);
    bool solve(Inequality goal);
    bool solve(string model1, string model2);
    void reset();

    bool appendProofNodes(ProofRule rule, shared_ptr<ProofNode> leftNode, shared_ptr<ProofNode> rightNode);
    shared_ptr<ProofNode> newChildProofNode(shared_ptr<ProofNode> node);
    void learnGoal(shared_ptr<ProofNode> node);
    void closeCurrentGoal();
    void dismissCurrentGoal();

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
    bool consRule(shared_ptr<ProofNode> node);
    bool weakRightRule(shared_ptr<ProofNode> node);
    bool weakLeftRule(shared_ptr<ProofNode> node);
    bool loopRule(shared_ptr<ProofNode> node);

    bool invcapEmptyRule(shared_ptr<ProofNode> node);
    bool idseqEmptyRule(shared_ptr<ProofNode> node);

    void log(string message, int requiredLevel);
    string toDotFormat(shared_ptr<ProofNode> node, shared_ptr<ProofNode> currentGoal);
    static shared_ptr<ProofNode> root;
    static shared_ptr<Solver> rootSolver;
    void exportProof(string filename, shared_ptr<ProofNode> root = Solver::root);
};

// TODO remove: typedef void (*ProofRule)(ProofNode &);