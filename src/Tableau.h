#pragma once
#include <stack>
#include <queue>
#include <memory>
#include <fstream>
#include "Relation.h"
#include "Metastatement.h"

using namespace std;

class Tableau
{
public:
    class Node
    {
    public:
        Node(Tableau *tableau, shared_ptr<Relation> relation);
        Node(Tableau *tableau, shared_ptr<Metastatement> metastatement);

        Tableau *tableau;
        shared_ptr<Relation> relation = nullptr;
        shared_ptr<Metastatement> metastatement = nullptr;
        shared_ptr<Node> leftNode = nullptr;
        shared_ptr<Node> rightNode = nullptr;
        Node *parentNode = nullptr;
        bool closed = false;

        bool isClosed();
        bool isLeaf() const;
        void appendBranches(shared_ptr<Relation> leftRelation, shared_ptr<Relation> rightRelation = nullptr);
        void appendBranches(shared_ptr<Metastatement> metastatement);

        void toDotFormat(ofstream &output) const;

        struct CompareNodes
        {
            bool operator()(const shared_ptr<Node> left, const shared_ptr<Node> right) const;
        };
    };

    Tableau(initializer_list<shared_ptr<Relation>> initalRelations);
    Tableau(vector<shared_ptr<Relation>> initalRelations);

    shared_ptr<Node> rootNode;
    priority_queue<shared_ptr<Node>, vector<shared_ptr<Node>>, Node::CompareNodes> unreducedNodes;

    bool applyRule(shared_ptr<Node> node);
    bool solve(int bound = 30);

    // methods for regular reasoning
    vector<vector<shared_ptr<Relation>>> DNF();
    bool applyModalRule();
    vector<shared_ptr<Relation>> calcReuqest();

    void toDotFormat(ofstream &output) const;
    void exportProof(string filename) const;
};
