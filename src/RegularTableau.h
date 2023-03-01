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

        unordered_set<shared_ptr<Relation>> relation;
        tuple<shared_ptr<Node>> childNodes;
        Node *parentNode = nullptr;
        bool closed = false;

        bool isClosed();
        void append(initializer_list<shared_ptr<Node>> childNodes));

        void toDotFormat(ofstream &output) const;
    };

    RegularTableau(initializer_list<shared_ptr<Node>> initalNodes);
    ~RegularTableau();

    tuple<shared_ptr<Node>> rootNodes;
    stack<shared_ptr<Node>> unreducedNodes;

    unordered_set<unordered_set<shared_ptr<Relation>>> DNF(unordered_set<shared_ptr<Relation>> clause);
    void applyRule(shared_ptr<Node> node);
    bool solve(int bound = 30);

    void toDotFormat(ofstream &output) const;
};
