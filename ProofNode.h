#pragma once
#include <unordered_set>
#include "Relation.h"
#include "ProofRule.h"

using namespace std;

enum class ProofNodeStatus
{
    none,
    closed,
    open
};

class ProofNode
{
public:
    ProofNode();
    ~ProofNode();

    ProofNodeStatus status;
    ProofRule appliedRule;
    shared_ptr<ProofNode> leftNode; // left and rigth children in proof tree
    shared_ptr<ProofNode> rightNode;
    shared_ptr<ProofNode> parent;

    RelationSet left;
    RelationSet right;

    string toDotFormat();

    bool operator==(const ProofNode &other) const;
};

typedef unordered_set<shared_ptr<ProofNode>> Theory;