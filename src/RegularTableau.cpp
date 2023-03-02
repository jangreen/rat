#include <iostream>
#include "RegularTableau.h"
#include "Tableau.h"

// hashing
// using boost::hash_combine
template <class T>
inline void hash_combine(std::size_t &seed, T const &v)
{
    seed ^= std::hash<T>()(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

size_t hash<RegularTableau::Node>::operator()(const RegularTableau::Node &node) const
{
    size_t seed = 0;
    for (auto relation : node.relations)
    {
        hash_combine(seed, relation->toString()); // TODO: use string reprstation
    }
    return seed;
}

size_t RegularTableau::Node::Hash::operator()(const shared_ptr<Node> node) const
{
    return hash<Node>()(*node);
}

bool RegularTableau::Node::Equal::operator()(const shared_ptr<Node> node1, const shared_ptr<Node> node2) const
{
    return *node1 == *node2;
}

using namespace std;

RegularTableau::Node::Node(initializer_list<shared_ptr<Relation>> relations) : relations(relations) {}
RegularTableau::Node::Node(vector<shared_ptr<Relation>> relations) : relations(relations) {}

RegularTableau::Node::~Node() {}

void RegularTableau::Node::toDotFormat(ofstream &output) const
{
    output << "N" << this
           << "[label=\"";
    for (auto relation : relations)
    {
        output << relation->toString() << endl;
    }
    output << "\"";
    // color closed branches
    if (closed)
    {
        output << ", color=green";
    }
    output
        << "];" << endl;
    // children
    for (auto childNode : childNodes)
    {
        childNode->toDotFormat(output);
        output << "N" << this << " -> "
               << "N" << childNode << ";" << endl;
    }
}

RegularTableau::RegularTableau(initializer_list<shared_ptr<Node>> initalNodes)
{
    rootNodes = initalNodes;
    for (auto node : initalNodes)
    {
        nodes.emplace(node);
        unreducedNodes.push(node);
    }
}
RegularTableau::~RegularTableau() {}

vector<vector<shared_ptr<Relation>>> RegularTableau::DNF(vector<shared_ptr<Relation>> clause)
{
    Tableau tableau{clause};
    return tableau.DNF();
}
void RegularTableau::expandNode(shared_ptr<Node> node)
{
    Tableau tableau{node->relations};
    tableau.exportProof("inital");
    tableau.applyModalRule();
    tableau.exportProof("modal");
    auto request = tableau.calcReuqest();
    tableau.exportProof("request");
    auto dnf = DNF(request);
    for (auto clause : dnf)
    {
        addNode(node, clause);
    }
}

void RegularTableau::addNode(shared_ptr<Node> parent, vector<shared_ptr<Relation>> clause)
{
    shared_ptr<Relation> positiveRelation = nullptr;
    for (auto literal : clause)
    {
        if (!literal->negated)
        {
            positiveRelation = literal;
            break;
        }
    }
    // calculate renaming & rename
    vector<int> renaming = positiveRelation->calculateRenaming();
    for (auto literal : clause)
    {
        literal->rename(renaming);
    }
    // create node, edges, push to unreduced nodes
    shared_ptr<Node> newNode = make_shared<Node>(clause);
    for (auto literal : clause)
    {
        cout << literal->toString() << " # ";
    }

    cout << hash<Node>()(*newNode) << endl;
    cout << Node::Hash()(newNode) << endl;
    cout << (nodes.find(newNode) != nodes.end()) << endl;
    for (auto node : nodes)
    {
        cout << hash<Node>()(*node) << endl;
    }
}

bool RegularTableau::solve()
{
    while (!unreducedNodes.empty())
    {
        auto currentNode = unreducedNodes.top();
        unreducedNodes.pop();
        expandNode(currentNode);
    }
}

void RegularTableau::toDotFormat(ofstream &output) const
{
    output << "digraph {" << endl
           << "node[shape=\"box\"]" << endl;
    for (auto rootNode : rootNodes)
    {
        rootNode->toDotFormat(output);
    }
    output << "}" << endl;
}

bool RegularTableau::Node::operator==(const Node &otherNode) const
{
    // <=
    for (auto relation : relations)
    {
        if (find_if(otherNode.relations.begin(), otherNode.relations.end(), [relation](const shared_ptr<Relation> comparedRelation)
                    { return *comparedRelation == *relation; }) == otherNode.relations.end())
        {
            return false;
        }
    }
    // =>
    for (auto relation : otherNode.relations)
    {
        if (find_if(relations.begin(), relations.end(), [relation](const shared_ptr<Relation> comparedRelation)
                    { return *comparedRelation == *relation; }) == relations.end())
        {
            return false;
        }
    }
    return true;
}