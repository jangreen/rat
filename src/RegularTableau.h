#pragma once
#include <stack>
#include <memory>
#include <fstream>
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
        ~Node();

        unordered_set<shared_ptr<Relation>> relations;
        tuple<shared_ptr<Node>> childNodes;
        Node *parentNode = nullptr;
        bool closed = false;

        bool isClosed();
        void append(initializer_list<shared_ptr<Node>> childNodes);

        void toDotFormat(ofstream &output) const;
    };

    RegularTableau(initializer_list<shared_ptr<Node>> initalNodes);
    ~RegularTableau();

    unordered_set<shared_ptr<Node>> rootNodes;
    stack<shared_ptr<Node>> unreducedNodes;

    static vector<vector<shared_ptr<Relation>>> DNF(vector<shared_ptr<Relation>> clause);
    void expandNode();
    bool solve();

    void toDotFormat(ofstream &output) const;
};
