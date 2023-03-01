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

vector<vector<shared_ptr<Relation>>> RegularTableau::DNF(vector<shared_ptr<Relation>> clause)
{
    Tableau tableau{clause};
    return tableau.DNF();
}
