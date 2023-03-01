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
        Node(shared_ptr<Relation> relation);
        Node(shared_ptr<Metastatement> metastatement);
        ~Node();

        shared_ptr<Relation> relation;
        shared_ptr<Metastatement> metastatement;
        shared_ptr<Node> leftNode;
        shared_ptr<Node> rightNode;
        Node *parentNode = nullptr;
        bool closed = false;

        bool isClosed();
        bool isLeaf() const;
        void appendBranches(shared_ptr<Node> leftNode, shared_ptr<Node> rightNode = nullptr);

        void toDotFormat(ofstream &output) const;
    };

    Tableau(initializer_list<shared_ptr<Relation>> initalRelations);
    Tableau(unordered_set<shared_ptr<Relation>> initalRelations);
    ~Tableau();

    shared_ptr<Node> rootNode;
    stack<shared_ptr<Node>> unreducedNodes;

    void applyRule(shared_ptr<Node> node);
    bool solve(int bound = 30);

    void toDotFormat(ofstream &output) const;
};
