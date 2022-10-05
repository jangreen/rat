#pragma once
#include <unordered_set>
#include "Relation.h"

using namespace std;

class ProofNode
{
public:
    ProofNode();
    ~ProofNode();

    string toDotFormat();

    string appliedRule;
    bool closed;                    // true iff subtree has proof
    shared_ptr<ProofNode> leftNode; // left and rigth children in proof tree
    shared_ptr<ProofNode> rightNode;

    RelationSet left;
    RelationSet right;
};
