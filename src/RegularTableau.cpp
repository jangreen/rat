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

shared_ptr<Relation> RegularTableau::Node::saturateRelation(shared_ptr<Relation> relation)
{
    if (relation == nullptr || relation->saturated)
    {
        return nullptr;
    }
    if (relation->label && relation->operation == Operation::none && relation->identifier && *relation->identifier != "id" && *relation->identifier != "0") // TODO: operation::id operation::empty native
    {
        string baseRelation = *relation->identifier;
        for (auto assumption : assumptions)
        {
            if (assumption->type == AssumptionType::regular && *assumption->baseRelation == baseRelation)
            {
                shared_ptr<Relation> leftSide = make_shared<Relation>(*assumption->relation);
                leftSide->saturated = true;
                leftSide->label = relation->label;
                leftSide->negated = true;
                return leftSide;
            }
        }
        return nullptr;
    }
    shared_ptr<Relation> leftSaturated = saturateRelation(relation->leftOperand);
    shared_ptr<Relation> rightSaturated = saturateRelation(relation->rightOperand);
    if (leftSaturated == nullptr && rightSaturated == nullptr)
    {
        return nullptr;
    }
    if (leftSaturated == nullptr)
    {
        leftSaturated = relation->leftOperand;
    }
    if (rightSaturated == nullptr)
    {
        rightSaturated = relation->rightOperand;
    }
    shared_ptr<Relation> saturated = make_shared<Relation>(relation->operation, leftSaturated, rightSaturated);
    saturated->negated = true;
    return saturated;
}

void RegularTableau::Node::saturate()
{
    vector<shared_ptr<Relation>> saturatedRelations;
    for (auto relation : relations)
    {
        if (relation->negated)
        {
            auto saturated = saturateRelation(relation);
            if (saturated != nullptr)
            {
                saturatedRelations.push_back(saturated);
            }
        }
    }
    relations.insert(relations.end(), saturatedRelations.begin(), saturatedRelations.end());
}

void RegularTableau::Node::toDotFormat(ofstream &output)
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
    // edges
    for (auto &[childNode, edgeLabel] : childNodes)
    {
        output << "N" << this << " -> "
               << "N" << childNode << ";" << endl;
    }
    printed = true;

    // children
    for (auto &[childNode, edgeLabel] : childNodes)
    {
        if (!childNode->printed)
        {
            childNode->toDotFormat(output);
        }
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
vector<shared_ptr<Assumption>> RegularTableau::assumptions;

vector<vector<shared_ptr<Relation>>> RegularTableau::DNF(vector<shared_ptr<Relation>> clause)
{
    Tableau tableau{clause};
    return tableau.DNF();
}
void RegularTableau::expandNode(shared_ptr<Node> node)
{
    Tableau tableau{node->relations};
    tableau.exportProof("inital");
    bool expandable = tableau.applyModalRule();
    if (expandable)
    {
        tableau.exportProof("modal");
        auto request = tableau.calcReuqest();
        tableau.exportProof("request");
        auto dnf = DNF(request);
        for (auto clause : dnf)
        {
            addNode(node, clause);
        }
    }
    else
    {
        // TODO:
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

    // saturation phase (assumptions)
    // newNode->saturate();

    auto existingNode = nodes.find(newNode);
    if (existingNode != nodes.end())
    {
        // equivalent node exists
        tuple<shared_ptr<Node>, vector<int>> edge{existingNode->get(), renaming};
        parent->childNodes.push_back(edge);
    }
    else
    {
        // equivalent node does not exist
        tuple<shared_ptr<Node>, vector<int>> edge{newNode, renaming};
        parent->childNodes.push_back(edge);
        nodes.emplace(newNode);
        unreducedNodes.push(newNode);
    }
}

bool RegularTableau::solve()
{
    while (!unreducedNodes.empty())
    {
        auto currentNode = unreducedNodes.top();
        unreducedNodes.pop();
        ofstream file2("reg.dot");
        toDotFormat(file2);
        expandNode(currentNode);
    }
}

void RegularTableau::toDotFormat(ofstream &output) const
{
    for (auto node : nodes)
    { // reset printed property
        node->printed = false;
    }
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