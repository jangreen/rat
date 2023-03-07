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
RegularTableau::Node::Node(Clause relations) : relations(relations) {}

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

RegularTableau::RegularTableau(initializer_list<shared_ptr<Relation>> initalRelations) : RegularTableau(vector(initalRelations)) {}
RegularTableau::RegularTableau(Clause initalRelations)
{
    auto dnf = DNF(initalRelations);
    cout << "Inital DNF: ";
    for (auto clause : dnf)
    {
        cout << "  ||  ";
        for (auto literal : clause)
            cout << literal->toString() << " and ";
    }
    cout << endl;

    for (auto clause : dnf)
    {
        addNode(nullptr, clause);
    }
    // exportProof("initalized");
}

vector<shared_ptr<Assumption>> RegularTableau::assumptions;

vector<Clause> RegularTableau::DNF(Clause clause)
{
    Tableau tableau{clause};
    return tableau.DNF();
}

// node has only normal terms
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
        if (tableau.rootNode->isClosed())
        {
            // can happen with empty hypotheses
            node->closed = true;
            return;
        }
        auto dnf = DNF(request);
        for (auto clause : dnf)
        {
            addNode(node, clause);
        }
        // TODO: if dnf is empty close node
    }
    else
    {
        // TODO:
    }
}

// clause is in normal form
// parent == nullptr -> rootNode
void RegularTableau::addNode(shared_ptr<Node> parent, Clause clause)
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

    // saturation phase
    saturate(clause);
    auto saturatedDNF = DNF(clause);
    for (auto saturatedClause : saturatedDNF)
    {
        // create node, edges, push to unreduced nodes
        shared_ptr<Node> newNode = make_shared<Node>(saturatedClause);

        auto existingNode = nodes.find(newNode);
        if (existingNode != nodes.end())
        {
            // equivalent node exists
            if (parent != nullptr)
            {
                tuple<shared_ptr<Node>, vector<int>> edge{existingNode->get(), renaming};
                parent->childNodes.push_back(edge);
            }
        }
        else
        {
            // equivalent node does not exist
            if (parent != nullptr)
            {
                tuple<shared_ptr<Node>, vector<int>> edge{newNode, renaming};
                parent->childNodes.push_back(edge);
                nodes.emplace(newNode);
                unreducedNodes.push(newNode);
            }
            else
            {
                // new root node
                nodes.emplace(newNode);
                unreducedNodes.push(newNode);
                rootNodes.push_back(newNode);
            }
        }
    }
}

shared_ptr<Relation> RegularTableau::saturateRelation(shared_ptr<Relation> relation)
{
    if (relation == nullptr || relation->saturated || relation->operation == Operation::none || relation->operation == Operation::identity || relation->operation == Operation::empty)
    {
        return nullptr;
    }
    if (relation->label && relation->operation == Operation::base)
    {
        string baseRelation = *relation->identifier;
        for (auto assumption : assumptions) // TODO fast get regular assumption
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

shared_ptr<Relation> RegularTableau::saturateIdRelation(shared_ptr<Assumption> assumption, shared_ptr<Relation> relation)
{
    if (relation == nullptr || relation->saturated || relation->operation == Operation::identity || relation->operation == Operation::empty)
    {
        return nullptr;
    }
    if (relation->label)
    {
        shared_ptr<Relation> copy = make_shared<Relation>(*relation);
        copy->label = nullopt;
        copy->negated = false;
        shared_ptr<Relation> assumptionR = make_shared<Relation>(*assumption->relation);
        assumptionR->saturated = true;
        assumptionR->label = relation->label;
        shared_ptr<Relation> r = make_shared<Relation>(Operation::composition, assumptionR, copy);
        r->negated = true;
        return r;
    }
    shared_ptr<Relation> leftSaturated = saturateIdRelation(assumption, relation->leftOperand);
    shared_ptr<Relation> rightSaturated = saturateIdRelation(assumption, relation->rightOperand);
    // TODO_ support intersections -> different identity or no saturations
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

void RegularTableau::saturate(Clause &clause)
{
    Clause saturatedRelations;
    // regular assumptions
    for (auto literal : clause)
    {
        if (literal->negated)
        {
            auto saturated = saturateRelation(literal);
            if (saturated != nullptr)
            {
                saturatedRelations.push_back(saturated);
            }
        }
    }

    for (auto assumption : assumptions)
    {
        vector<int> nodeLabels;
        switch (assumption->type)
        {
        case AssumptionType::empty:
            for (auto literal : clause)
            {
                for (auto label : literal->labels())
                {
                    if (find(nodeLabels.begin(), nodeLabels.end(), label) == nodeLabels.end())
                    {
                        nodeLabels.push_back(label);
                    }
                }
            }

            for (auto label : nodeLabels)
            {
                shared_ptr<Relation> leftSide = make_shared<Relation>(*assumption->relation);
                shared_ptr<Relation> full = make_shared<Relation>(Operation::full);
                shared_ptr<Relation> r = make_shared<Relation>(Operation::composition, leftSide, full);
                r->saturated = true;
                r->label = label;
                r->negated = true;
                saturatedRelations.push_back(r);
            }
            break;
        case AssumptionType::identity:
            for (auto literal : clause)
            {
                if (literal->negated)
                {
                    auto saturated = saturateIdRelation(assumption, literal);
                    if (saturated != nullptr)
                    {
                        saturatedRelations.push_back(saturated);
                    }
                }
            }
        default:
            break;
        }
    }

    clause.insert(clause.end(), saturatedRelations.begin(), saturatedRelations.end());
}

bool RegularTableau::solve()
{
    while (!unreducedNodes.empty())
    {
        auto currentNode = unreducedNodes.top();
        unreducedNodes.pop();
        /* TODO: remove: */
        exportProof("reg");
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

void RegularTableau::exportProof(string filename) const
{
    ofstream file(filename + ".dot");
    toDotFormat(file);
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