#pragma once
#include <unordered_set>
#include "Relation.h"
#include "ProofRule.h"

using namespace std;

class ProofNode
{
public:
    ProofNode();
    ~ProofNode();

    ProofRule appliedRule;
    shared_ptr<ProofNode> leftNode;  // left and right children in proof tree
    shared_ptr<ProofNode> rightNode; // nullable
    shared_ptr<ProofNode> parent;

    RelationSet literals;

    static string getId(ProofNode &node);
    string toDotFormat(shared_ptr<ProofNode> CurrentGoal);
    string relationString();

    bool operator==(const ProofNode &other) const;
};

typedef unordered_set<shared_ptr<ProofNode>> Theory;
typedef shared_ptr<ProofNode> Inequality;