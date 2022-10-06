#pragma once
#include <unordered_set>
#include "Relation.h"
#include "ProofNode.h"

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
    string appliedRule;
    shared_ptr<ProofNode> leftNode; // left and rigth children in proof tree
    shared_ptr<ProofNode> rightNode;

    RelationSet left;
    RelationSet right;

    string toDotFormat();
};

typedef unordered_set<shared_ptr<ProofNode>> Theory;