#pragma once
#include <unordered_set>
#include "Relation.h"
#include "ProofRule.h"

using namespace std;

enum class ProofNodeStatus
{
    none,
    closed,
    open,
    dismiss
};

class ProofNode
{
public:
    ProofNode();
    ProofNode(const string &leftExpr, const string &rightExpr);
    ~ProofNode();

    ProofNodeStatus status;
    ProofRule appliedRule;
    shared_ptr<ProofNode> leftNode; // left and rigth children in proof tree
    shared_ptr<ProofNode> rightNode;
    shared_ptr<ProofNode> parent;
    // TODO: rename to allowed remaining cons application
    int currentConsDepth; // ierative deepening starting with zero then increase

    RelationSet left;
    RelationSet right;

    string toDotFormat(shared_ptr<ProofNode> CurrentGoal);
    string relationString();

    bool operator==(const ProofNode &other) const;
};

typedef unordered_set<shared_ptr<ProofNode>> Theory;
typedef shared_ptr<ProofNode> Inequality;