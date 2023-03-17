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
        Node(Tableau *tableau, const Relation &&relation);
        Node(Tableau *tableau, const Metastatement &&metastatement);

        Tableau *tableau;
        optional<Relation> relation = nullopt;
        optional<Metastatement> metastatement = nullopt;
        shared_ptr<Node> leftNode = nullptr;
        shared_ptr<Node> rightNode = nullptr;
        Node *parentNode = nullptr;
        Node *parentMetastatement = nullptr; // metastatement chain
        bool closed = false;

        bool isClosed();
        bool isLeaf() const;
        void appendBranches(const Relation &leftRelation, const Relation &rightRelation);
        void appendBranches(const Relation &leftRelation);
        void appendBranches(const Metastatement &metastatement);

        void toDotFormat(ofstream &output) const;

        struct CompareNodes
        {
            bool operator()(const shared_ptr<Node> left, const shared_ptr<Node> right) const;
        };
    };

    Tableau(initializer_list<Relation> initalRelations);
    Tableau(Clause initalRelations);

    shared_ptr<Node> rootNode;
    priority_queue<shared_ptr<Node>, vector<shared_ptr<Node>>, Node::CompareNodes> unreducedNodes;

    bool applyRule(shared_ptr<Node> node);
    bool solve(int bound = 30);

    // methods for regular reasoning
    vector<Clause> DNF();
    bool applyModalRule();
    Clause calcReuqest();

    void toDotFormat(ofstream &output) const;
    void exportProof(string filename) const;
};
