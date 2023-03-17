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
    Clause copy = node.relations;
    sort(copy.begin(), copy.end());
    for (const auto &relation : copy)
    {
        hash_combine(seed, relation.toString()); // TODO: use string reprstation
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

RegularTableau::Node::Node(initializer_list<Relation> relations) : relations(relations) {}
RegularTableau::Node::Node(Clause relations) : relations(relations) {}

void RegularTableau::Node::toDotFormat(ofstream &output)
{
    output << "N" << this
           << "[label=\"";
    output << hash<Node>()(*this) << endl;
    for (const auto &relation : relations)
    {
        output << relation.toString() << endl;
    }
    output << "\"";
    // color closed branches
    if (closed)
    {
        output << ", color=green";
    }
    // TODO: mark root nodes if (re)
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

RegularTableau::RegularTableau(initializer_list<Relation> initalRelations) : RegularTableau(vector(initalRelations)) {}
RegularTableau::RegularTableau(Clause initalRelations)
{
    auto dnf = DNF(initalRelations);
    for (auto clause : dnf)
    {
        addNode(nullptr, clause);
    }
    // TODO: remove exportProof("initalized");
}

vector<Assumption> RegularTableau::assumptions;

vector<Clause> RegularTableau::DNF(const Clause &clause)
{
    Tableau tableau{clause};
    return tableau.DNF();
}

// node has only normal terms
bool RegularTableau::expandNode(shared_ptr<Node> node)
{
    Tableau tableau{node->relations};
    tableau.exportProof("dnfcalc"); // initial
    bool expandable = tableau.applyModalRule();
    if (expandable)
    {
        tableau.exportProof("dnfcalc"); // modal
        auto request = tableau.calcReuqest();
        tableau.exportProof("dnfcalc"); // request
        if (tableau.rootNode->isClosed())
        {
            // can happen with empty hypotheses
            node->closed = true;
            return true;
        }
        auto dnf = DNF(request);
        for (const auto &clause : dnf)
        {
            addNode(node, clause);
        }
        // TODO: if dnf is empty close node
        /*cout << "--------" << endl;
        for (auto clause : dnf)
        {
            cout << "--" << endl;
            for (auto literal : clause)
            {
                cout << literal.toString() << endl;
            }
        }*/
        if (dnf.empty())
        {
            node->closed = true;
        }
        return true;
    }
    return false;
}

// clause is in normal form
// parent == nullptr -> rootNode
void RegularTableau::addNode(shared_ptr<Node> parent, Clause clause)
{
    unique_ptr<Relation> positiveRelation = nullptr;
    for (const auto &literal : clause)
    {
        if (!literal.negated)
        {
            positiveRelation = make_unique<Relation>(literal);
            break;
        }
    }
    // calculate renaming & rename
    vector<int> renaming = positiveRelation->calculateRenaming();
    for (Relation &literal : clause)
    {
        literal.rename(renaming);
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
            }
            else
            {
                // new root node
                rootNodes.push_back(newNode);
            }
            nodes.emplace(newNode);
            unreducedNodes.push(newNode);
        }
    }
}

optional<Relation> RegularTableau::saturateRelation(const Relation &relation)
{
    if (relation.saturated)
    {
        return nullopt;
    }

    optional<Relation> leftSaturated;
    optional<Relation> rightSaturated;

    switch (relation.operation)
    {
    case Operation::none:
        return nullopt;
    case Operation::identity:
        return nullopt;
    case Operation::empty:
        return nullopt;
    case Operation::base:
        for (const auto &assumption : assumptions) // TODO fast get regular assumption
        {
            if (assumption.type == AssumptionType::regular && *assumption.baseRelation == *relation.identifier)
            {
                Relation leftSide = Relation(*assumption.relation);
                leftSide.label = relation.label;
                leftSide.negated = true;
                return leftSide;
            }
        }
        return nullopt;
    case Operation::choice:
    case Operation::intersection:
        leftSaturated = saturateRelation(*relation.leftOperand);
        rightSaturated = saturateRelation(*relation.rightOperand);
        break;
    case Operation::composition:
    case Operation::converse:
    case Operation::transitiveClosure:
        leftSaturated = saturateRelation(*relation.leftOperand);
        break;
    default:
        break;
    }
    if (!leftSaturated && !rightSaturated)
    {
        return nullopt;
    }
    if (!leftSaturated)
    {
        leftSaturated = Relation(*relation.leftOperand);
    }
    if (!rightSaturated && relation.rightOperand != nullptr)
    {
        rightSaturated = Relation(*relation.rightOperand);
    }
    if (rightSaturated)
    {
        Relation saturated = Relation(relation.operation, std::move(*leftSaturated), std::move(*rightSaturated));
        saturated.negated = true;
        return saturated;
    }
    Relation saturated = Relation(relation.operation, std::move(*leftSaturated));
    saturated.negated = true;
    return saturated;
}

unique_ptr<Relation> RegularTableau::saturateIdRelation(const Assumption &assumption, const Relation &relation)
{
    if (relation.saturated || relation.operation == Operation::identity || relation.operation == Operation::empty)
    {
        return nullptr;
    }
    if (relation.label)
    {
        unique_ptr<Relation> copy = make_unique<Relation>(relation);
        copy->label = nullopt;
        copy->negated = false;
        unique_ptr<Relation> assumptionR = make_unique<Relation>(*assumption.relation);
        assumptionR->saturated = true;
        assumptionR->label = relation.label;
        unique_ptr<Relation> r = make_unique<Relation>(Operation::composition, std::move(*assumptionR), std::move(*copy));
        r->negated = true;
        return r;
    }
    unique_ptr<Relation> leftSaturated = saturateIdRelation(assumption, *relation.leftOperand);
    unique_ptr<Relation> rightSaturated = saturateIdRelation(assumption, *relation.rightOperand);
    // TODO_ support intersections -> different identity or no saturations
    if (leftSaturated == nullptr && rightSaturated == nullptr)
    {
        return nullptr;
    }
    if (leftSaturated == nullptr)
    {
        leftSaturated = make_unique<Relation>(*relation.leftOperand);
    }
    if (rightSaturated == nullptr)
    {
        rightSaturated = make_unique<Relation>(*relation.rightOperand);
    }
    unique_ptr<Relation> saturated = make_unique<Relation>(relation.operation, std::move(*leftSaturated), std::move(*rightSaturated));
    saturated->negated = true;
    return saturated;
}

void RegularTableau::saturate(Clause &clause)
{
    // return; // TODO remove:
    Clause saturatedRelations;
    // regular assumptions
    for (const Relation &literal : clause)
    {
        if (literal.negated)
        {
            optional<Relation> saturated = saturateRelation(literal);
            if (saturated)
            {
                saturatedRelations.push_back(std::move(*saturated));
            }
        }
    }

    for (const auto &assumption : assumptions)
    {
        vector<int> nodeLabels;
        switch (assumption.type)
        {
        case AssumptionType::empty:
            for (const auto &literal : clause)
            {
                for (auto label : literal.labels())
                {
                    if (find(nodeLabels.begin(), nodeLabels.end(), label) == nodeLabels.end())
                    {
                        nodeLabels.push_back(label);
                    }
                }
            }

            for (auto label : nodeLabels)
            {
                Relation leftSide = Relation(*assumption.relation);
                Relation full = Relation(Operation::full);
                unique_ptr<Relation> r = make_unique<Relation>(Operation::composition, std::move(leftSide), std::move(full));
                r->saturated = true;
                r->label = label;
                r->negated = true;
                saturatedRelations.push_back(*r);
            }
            break;
        case AssumptionType::identity:
            for (const auto &literal : clause)
            {
                if (literal.negated)
                {
                    auto saturated = saturateIdRelation(assumption, literal);
                    if (saturated != nullptr)
                    {
                        saturatedRelations.push_back(*saturated);
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
        exportProof("regular");
        if (!expandNode(currentNode))
        {
            return false;
        }
    }
    return true;
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
        output << "root -> "
               << "N" << rootNode << ";" << endl;
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
    // shorcuts
    if (relations.size() != otherNode.relations.size())
    {
        return false;
    }
    // copy, sort, compare O(n log n)
    Clause c1 = relations;
    Clause c2 = otherNode.relations;
    std::sort(c1.begin(), c1.end());
    std::sort(c2.begin(), c2.end());
    return c1 == c2;
}