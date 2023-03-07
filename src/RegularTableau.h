#pragma once
#include <stack>
#include <memory>
#include <fstream>
#include <vector>
#include <unordered_set>
#include "Relation.h"
#include "Metastatement.h"
#include "Assumption.h"

using namespace std;

class RegularTableau
{
public:
    class Node
    {
    public:
        Node(initializer_list<shared_ptr<Relation>> relations);
        Node(Clause relations);

        Clause relations;
        vector<tuple<shared_ptr<Node>, vector<int>>> childNodes;
        Node *parentNode = nullptr;
        bool closed = false;

        bool printed = false; // prevent cycling in printing
        void toDotFormat(ofstream &output);

        bool operator==(const Node &otherNode) const;

        struct Hash
        {
            size_t operator()(const shared_ptr<Node> node) const;
        };

        struct Equal
        {
            bool operator()(const shared_ptr<Node> node1, const shared_ptr<Node> node2) const;
        };
    };

    RegularTableau(initializer_list<shared_ptr<Relation>> initalRelations);
    RegularTableau(Clause initalRelations);

    vector<shared_ptr<Node>> rootNodes;
    unordered_set<shared_ptr<Node>, Node::Hash, Node::Equal> nodes;
    stack<shared_ptr<Node>> unreducedNodes;
    static vector<shared_ptr<Assumption>> assumptions;

    static vector<Clause> DNF(Clause clause);
    void expandNode(shared_ptr<Node> node);
    void addNode(shared_ptr<Node> parent, Clause clause); // TODO: move in node class, call on parent
    shared_ptr<Relation> saturateRelation(shared_ptr<Relation> relation);
    shared_ptr<Relation> saturateIdRelation(shared_ptr<Assumption> assumption, shared_ptr<Relation> relation);
    void saturate(Clause &clause);
    bool solve();

    void toDotFormat(ofstream &output) const;
    void exportProof(string filename) const;
};

namespace std
{
    template <>
    struct hash<RegularTableau::Node>
    {
        std::size_t operator()(const RegularTableau::Node &node) const;
    };
}