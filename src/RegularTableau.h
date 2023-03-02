#pragma once
#include <stack>
#include <memory>
#include <fstream>
#include <vector>
#include <unordered_set>
#include "Relation.h"
#include "Metastatement.h"

using namespace std;

class RegularTableau
{
public:
    class Node
    {
    public:
        Node(initializer_list<shared_ptr<Relation>> relations);
        Node(vector<shared_ptr<Relation>> relations);
        ~Node();

        vector<shared_ptr<Relation>> relations;
        vector<shared_ptr<Node>> childNodes;
        Node *parentNode = nullptr;
        bool closed = false;

        bool isClosed();
        void append(initializer_list<shared_ptr<Node>> childNodes);

        void toDotFormat(ofstream &output) const;

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

    RegularTableau(initializer_list<shared_ptr<Node>> initalNodes);
    ~RegularTableau();

    vector<shared_ptr<Node>> rootNodes;
    unordered_set<shared_ptr<Node>, Node::Hash, Node::Equal> nodes;
    stack<shared_ptr<Node>> unreducedNodes;

    static vector<vector<shared_ptr<Relation>>> DNF(vector<shared_ptr<Relation>> clause);
    void expandNode(shared_ptr<Node> node);
    void addNode(shared_ptr<Node> parent, vector<shared_ptr<Relation>> clause);
    bool solve();

    void toDotFormat(ofstream &output) const;
};

namespace std
{
    template <>
    struct hash<RegularTableau::Node>
    {
        std::size_t operator()(const RegularTableau::Node &node) const;
    };
}