#include <iostream>
#include "RegularTableau.h"
#include "Tableau.h"

using namespace std;

RegularTableau::Node::Node(initializer_list<shared_ptr<Relation>> relations) : relations(relations) {}
RegularTableau::Node::~Node() {}

RegularTableau::RegularTableau(initializer_list<shared_ptr<Node>> initalNodes)
{
    rootNodes = initalNodes;
    for (auto node : initalNodes)
    {
        unreducedNodes.push(node);
    }
}
RegularTableau::~RegularTableau() {}

unordered_set<unordered_set<shared_ptr<Relation>>> RegularTableau::DNF(unordered_set<shared_ptr<Relation>> clause)
{
    Tableau tableau{clause};
}
