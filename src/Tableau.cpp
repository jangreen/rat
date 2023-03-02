#include <iostream>
#include "Tableau.h"
#include "ProofRule.h"
#include "Metastatement.h"

using namespace std;

/* RULE IMPLEMENTATIONS */
// TODO generice function: mayb binaryRule, ...
optional<tuple<shared_ptr<Relation>, shared_ptr<Relation>>> choiceRule(shared_ptr<Relation> relation)
{
    if (!relation->label)
    {
        // case: intersection or composition (only cases for labeled terms that can happen)
        auto left = choiceRule(relation->leftOperand);
        if (left)
        {
            const auto &[subrelation1, subrelation2] = *left;
            tuple<shared_ptr<Relation>, shared_ptr<Relation>> result{
                make_shared<Relation>(relation->operation, subrelation1, relation->rightOperand),
                make_shared<Relation>(relation->operation, subrelation2, relation->rightOperand)};
            return result;
        }

        // only intersection
        if (relation->operation != Operation::intersection)
        {
            return nullopt;
        }
        auto right = choiceRule(relation->rightOperand);
        if (right)
        {
            const auto &[subrelation1, subrelation2] = *right;
            tuple<shared_ptr<Relation>, shared_ptr<Relation>> result{
                make_shared<Relation>(relation->operation, relation->leftOperand, subrelation1),
                make_shared<Relation>(relation->operation, relation->leftOperand, subrelation2)};
            return result;
        }
        return nullopt;
    }
    else if (relation->operation == Operation::choice)
    {
        // Rule::choice, Rule::negChoice
        shared_ptr<Relation> r1 = make_shared<Relation>(*relation->leftOperand);
        r1->label = relation->label;
        shared_ptr<Relation> r2 = make_shared<Relation>(*relation->rightOperand);
        r2->label = relation->label;
        tuple<shared_ptr<Relation>, shared_ptr<Relation>> result{r1, r2};
        return result;
    }
    return nullopt;
}

optional<tuple<shared_ptr<Relation>, shared_ptr<Relation>>> transitiveClosureRule(shared_ptr<Relation> relation)
{
    if (!relation->label)
    {
        // case: intersection or composition (only cases for labeled terms that can happen)
        auto left = transitiveClosureRule(relation->leftOperand);
        if (left)
        {
            const auto &[subrelation1, subrelation2] = *left;
            tuple<shared_ptr<Relation>, shared_ptr<Relation>> result{
                make_shared<Relation>(relation->operation, subrelation1, relation->rightOperand),
                make_shared<Relation>(relation->operation, subrelation2, relation->rightOperand)};
            return result;
        }

        // only intersection
        if (relation->operation != Operation::intersection)
        {
            return nullopt;
        }
        auto right = transitiveClosureRule(relation->rightOperand);
        if (right)
        {
            const auto &[subrelation1, subrelation2] = *right;
            tuple<shared_ptr<Relation>, shared_ptr<Relation>> result{
                make_shared<Relation>(relation->operation, relation->leftOperand, subrelation1),
                make_shared<Relation>(relation->operation, relation->leftOperand, subrelation2)};
            return result;
        }
        return nullopt;
    }
    else if (relation->operation == Operation::transitiveClosure)
    {
        // Rule::transitiveClosure, Rule::negTransitiveClosure
        shared_ptr<Relation> r1 = make_shared<Relation>(*relation->leftOperand);
        r1->label = relation->label;
        shared_ptr<Relation> r12 = make_shared<Relation>(*relation);
        r12->label = nullopt;
        r12->negated = false;
        shared_ptr<Relation> r2 = make_shared<Relation>(nullopt); // empty relation
        r2->label = relation->label;
        tuple<shared_ptr<Relation>, shared_ptr<Relation>>
            result{
                make_shared<Relation>(Operation::composition, r1, r12),
                r2};
        return result;
    }
    return nullopt;
}

optional<shared_ptr<Relation>> idRule(shared_ptr<Relation> relation)
{
    if (!relation->label)
    {
        // case: intersection or composition (only cases for labeled terms that can happen)
        auto left = idRule(relation->leftOperand);
        if (left)
        {
            // case: composition && emtpy labeled relation -> move label to 'next' relation
            if (relation->operation == Operation::composition && (*left)->operation == Operation::none && !(*left)->identifier && (*left)->label)
            {
                shared_ptr<Relation> r1 = make_shared<Relation>(*relation->rightOperand);
                r1->label = (*left)->label;
                return r1;
            }
            return make_shared<Relation>(relation->operation, *left, relation->rightOperand);
        }
        // only intersection
        if (relation->operation != Operation::intersection)
        {
            return nullopt;
        }
        auto right = idRule(relation->rightOperand);
        if (right)
        {
            return make_shared<Relation>(relation->operation, relation->leftOperand, *right);
        }
        return nullopt;
    }
    else if (relation->operation == Operation::none && relation->identifier && *relation->identifier == "id") // TODO compare relations with overloaded == operator
    {
        // Rule::id, Rule::negId
        shared_ptr<Relation> r1 = make_shared<Relation>(nullopt); // empty relation
        r1->label = relation->label;
        return r1;
    }
    return nullopt;
}

optional<tuple<shared_ptr<Relation>, shared_ptr<Metastatement>>> aRule(shared_ptr<Relation> relation)
{
    if (!relation->label)
    {
        // case: intersection or composition (only cases for labeled terms that can happen)
        auto left = aRule(relation->leftOperand);
        if (left)
        {
            const auto &[subrelation1, metastatement] = *left;

            // case: composition && emtpy labeled relation -> move label to 'next' relation
            if (relation->operation == Operation::composition && subrelation1->operation == Operation::none && !subrelation1->identifier && subrelation1->label)
            {
                shared_ptr<Relation> r1 = make_shared<Relation>(*relation->rightOperand);
                r1->label = subrelation1->label;
                tuple<shared_ptr<Relation>, shared_ptr<Metastatement>> result{
                    r1,
                    metastatement};
                return result;
            }
            tuple<shared_ptr<Relation>, shared_ptr<Metastatement>> result{
                make_shared<Relation>(relation->operation, subrelation1, relation->rightOperand),
                metastatement};
            return result;
        }

        // only intersection
        if (relation->operation != Operation::intersection)
        {
            return nullopt;
        }
        auto right = aRule(relation->rightOperand);
        if (right)
        {
            const auto &[subrelation1, metastatement] = *right;
            tuple<shared_ptr<Relation>, shared_ptr<Metastatement>> result{
                make_shared<Relation>(relation->operation, relation->leftOperand, subrelation1),
                metastatement};
            return result;
        }
        return nullopt;
    }
    else if (relation->operation == Operation::none && relation->identifier && *relation->identifier != "id" && *relation->identifier != "0") // TODO compare relations with overloaded == operator
    {
        // Rule::a
        shared_ptr<Relation> r1 = make_shared<Relation>(nullopt); // empty relation
        Relation::maxLabel++;
        r1->label = Relation::maxLabel;
        tuple<shared_ptr<Relation>, shared_ptr<Metastatement>>
            result{
                r1,
                make_shared<Metastatement>(MetastatementType::labelRelation, *relation->label, Relation::maxLabel, *relation->identifier)};
        return result;
    }
    return nullopt;
}

optional<shared_ptr<Relation>> negARule(shared_ptr<Tableau::Node> node, shared_ptr<Relation> relation)
{
    if (!relation->label)
    {
        // case: intersection or composition (only cases for labeled terms that can happen)
        auto left = negARule(node, relation->leftOperand);
        if (left)
        {
            // case: composition && emtpy labeled relation -> move label to 'next' relation
            if (relation->operation == Operation::composition && (*left)->operation == Operation::none && !(*left)->identifier && (*left)->label)
            {
                shared_ptr<Relation> r1 = make_shared<Relation>(*relation->rightOperand);
                r1->label = (*left)->label;
                return r1;
            }
            return make_shared<Relation>(relation->operation, *left, relation->rightOperand);
        }
        // only intersection
        if (relation->operation != Operation::intersection)
        {
            return nullopt;
        }
        auto right = negARule(node, relation->rightOperand);
        if (right)
        {
            return make_shared<Relation>(relation->operation, relation->leftOperand, *right);
        }
        return nullopt;
    }
    else if (relation->operation == Operation::none && relation->identifier && *relation->identifier != "id" && *relation->identifier != "0") // TODO compare relations with overloaded == operator
    {
        // Rule::negA
        shared_ptr<Relation> r1 = make_shared<Relation>(nullopt); // empty relation
        int label = *relation->label;
        string baseRelation = *relation->identifier;
        Tableau::Node *currentNode = &(*node);
        while (currentNode != nullptr)
        {
            if (currentNode->metastatement != nullptr && currentNode->metastatement->type == MetastatementType::labelRelation)
            {
                if (currentNode->metastatement->label1 == label && currentNode->metastatement->baseRelation == baseRelation)
                {
                    r1->label = currentNode->metastatement->label2;
                    return r1;
                }
            }
            currentNode = currentNode->parentNode;
        }
    }
    return nullopt;
}

optional<shared_ptr<Relation>> compositionRule(shared_ptr<Relation> relation)
{
    if (!relation->label)
    {
        // case: intersection or composition (only cases for labeled terms that can happen)
        auto left = compositionRule(relation->leftOperand);
        if (left)
        {
            return make_shared<Relation>(relation->operation, *left, relation->rightOperand);
        }
        // only intersection
        if (relation->operation != Operation::intersection)
        {
            return nullopt;
        }
        auto right = compositionRule(relation->rightOperand);
        if (right)
        {
            return make_shared<Relation>(relation->operation, relation->leftOperand, *right);
        }
        return nullopt;
    }
    else if (relation->operation == Operation::composition)
    {
        // Rule::comnposition, Rule::negComposition
        shared_ptr<Relation> r1 = make_shared<Relation>(*relation->leftOperand);
        r1->label = relation->label;
        shared_ptr<Relation> r2 = make_shared<Relation>(*relation->rightOperand);
        return make_shared<Relation>(Operation::composition, r1, r2);
    }
    return nullopt;
}

optional<shared_ptr<Relation>> intersectionRule(shared_ptr<Relation> relation)
{
    if (!relation->label)
    {
        // case: intersection or composition (only cases for labeled terms that can happen)
        auto left = intersectionRule(relation->leftOperand);
        if (left)
        {
            return make_shared<Relation>(relation->operation, *left, relation->rightOperand);
        }
        // only intersection
        if (relation->operation != Operation::intersection)
        {
            return nullopt;
        }
        auto right = intersectionRule(relation->rightOperand);
        if (right)
        {
            return make_shared<Relation>(relation->operation, relation->leftOperand, *right);
        }
        return nullopt;
    }
    else if (relation->operation == Operation::intersection)
    {
        // Rule::inertsection, Rule::negIntersection
        shared_ptr<Relation> r1 = make_shared<Relation>(*relation->leftOperand);
        r1->label = relation->label;
        shared_ptr<Relation> r2 = make_shared<Relation>(*relation->rightOperand);
        r1->label = relation->label;
        return make_shared<Relation>(Operation::intersection, r1, r2);
    }
    return nullopt;
}
/* END RULE IMPLEMENTATIONS */

Tableau::Node::Node(Tableau *tableau, shared_ptr<Relation> relation) : tableau(tableau), relation(relation) {}
Tableau::Node::Node(Tableau *tableau, shared_ptr<Metastatement> metastatement) : tableau(tableau), metastatement(metastatement) {}
Tableau::Node::~Node() {}

bool Tableau::Node::isClosed()
{
    // Lazy evaluated closed nodes
    bool expanded = leftNode != nullptr;
    closed = closed || (expanded && leftNode->isClosed() && (rightNode == nullptr || rightNode->isClosed()));
    return closed;
};

bool Tableau::Node::isLeaf() const
{
    return leftNode == nullptr && rightNode == nullptr;
};

// helper
bool checkIfClosed(Tableau::Node *node, shared_ptr<Relation> relation)
{
    while (node != nullptr)
    {
        if (node->relation != nullptr && *node->relation == *relation && node->relation->negated != relation->negated)
        {
            return true;
        }
        node = node->parentNode;
    }
    return false;
}

void Tableau::Node::appendBranches(shared_ptr<Relation> leftRelation, shared_ptr<Relation> rightRelation)
{
    // do not expand closed branches
    if (isClosed())
    {
        return;
    }
    if (isLeaf())
    {
        if (leftRelation != nullptr)
        {
            auto leftNode = make_shared<Node>(tableau, leftRelation);
            this->leftNode = leftNode;
            leftNode->parentNode = this;
            leftNode->closed = checkIfClosed(this, leftRelation);
            tableau->unreducedNodes.push(leftNode);
        }
        if (rightRelation != nullptr)
        {
            auto rightNode = make_shared<Node>(tableau, rightRelation);
            this->rightNode = rightNode;
            rightNode->parentNode = this;
            rightNode->closed = checkIfClosed(this, rightRelation);
            tableau->unreducedNodes.push(rightNode);
        }
        return;
    }
    if (this->leftNode != nullptr)
    {
        this->leftNode->appendBranches(leftRelation, rightRelation);
    }
    if (this->rightNode != nullptr)
    {
        this->rightNode->appendBranches(leftRelation, rightRelation);
    }
}
// TODO DRY
void Tableau::Node::appendBranches(shared_ptr<Metastatement> metastatement)
{
    // do not expand closed branches
    if (isClosed())
    {
        return;
    }
    if (isLeaf())
    {
        if (metastatement != nullptr)
        {
            auto leftNode = make_shared<Node>(tableau, metastatement);
            this->leftNode = leftNode;
            leftNode->parentNode = this;
            tableau->unreducedNodes.push(leftNode);
        }
        return;
    }
    if (this->leftNode != nullptr)
    {
        this->leftNode->appendBranches(metastatement);
    }
    if (this->rightNode != nullptr)
    {
        this->rightNode->appendBranches(metastatement);
    }
}

void Tableau::Node::toDotFormat(ofstream &output) const
{
    output << "N" << this
           << "[label=\"";
    if (relation != nullptr)
    {
        output << relation->toString();
    }
    else
    {
        output << metastatement->toString();
    }
    output << "\"";
    // color closed branches
    if (closed)
    {
        output << ", fontcolor=green";
    }
    output
        << "];" << endl;
    // children
    if (leftNode != nullptr)
    {
        leftNode->toDotFormat(output);
        output << "N" << this << " -- "
               << "N" << leftNode << ";" << endl;
    }
    if (rightNode != nullptr)
    {
        rightNode->toDotFormat(output);
        output << "N" << this << " -- "
               << "N" << rightNode << ";" << endl;
    }
}

bool Tableau::Node::CompareNodes::operator()(const shared_ptr<Node> left, const shared_ptr<Node> right) const
{
    if (left->relation != nullptr && right->relation != nullptr)
    {
        return left->relation->negated < right->relation->negated;
    }
    else if (right->relation != nullptr)
    {
        return (left->metastatement != nullptr) < right->relation->negated;
    }
    return true;
}

Tableau::Tableau(initializer_list<shared_ptr<Relation>> initalRelations) : Tableau(vector(initalRelations)) {}
Tableau::Tableau(vector<shared_ptr<Relation>> initalRelations)
{
    shared_ptr<Node> currentNode = nullptr;
    for (auto relation : initalRelations)
    {
        shared_ptr<Node> newNode = make_shared<Node>(this, relation);
        if (rootNode == nullptr)
        {
            rootNode = newNode;
        }
        if (currentNode != nullptr)
        {
            currentNode->leftNode = newNode;
        }
        newNode->parentNode = &(*currentNode);
        currentNode = newNode;
        unreducedNodes.push(newNode);
    }
}
Tableau::~Tableau() {}

// helper
bool applyDNFRule(shared_ptr<Tableau::Node> node)
{
    if (node->relation == nullptr)
    {
        cout << "NULL: " << node->metastatement->toString() << endl;
    }
    // Rule::id, Rule::negId
    auto rId = idRule(node->relation);
    if (rId)
    {
        (*rId)->negated = node->relation->negated;
        node->appendBranches(*rId);
        return true;
    }
    // Rule::composition, Rule::negComposition
    auto rComposition = compositionRule(node->relation);
    if (rComposition)
    {
        (*rComposition)->negated = node->relation->negated;
        node->appendBranches(*rComposition);
        return true;
    }
    // Rule::intersection, Rule::negIntersection
    auto rIntersection = intersectionRule(node->relation);
    if (rIntersection)
    {
        (*rIntersection)->negated = node->relation->negated;
        node->appendBranches(*rIntersection);
        return true;
    }
    // Rule::choice, Rule::negChoice
    auto rChoice = choiceRule(node->relation);
    if (rChoice)
    {
        const auto &[r1, r2] = *rChoice;
        r1->negated = node->relation->negated;
        r2->negated = node->relation->negated;
        if (node->relation->negated)
        {
            node->appendBranches(r1);
            node->appendBranches(r2);
        }
        else
        {
            node->appendBranches(r1, r2);
        }
        return true;
    }
    // Rule::transitiveClosure, Rule::negTransitiveClosure
    auto rTransitiveClosure = transitiveClosureRule(node->relation);
    if (rTransitiveClosure)
    {
        const auto &[r1, r2] = *rTransitiveClosure;
        r1->negated = node->relation->negated;
        r2->negated = node->relation->negated;
        if (node->relation->negated)
        {
            node->appendBranches(r1);
            node->appendBranches(r2);
        }
        else
        {
            node->appendBranches(r1, r2);
        }
        return true;
    }
}

// helper
bool applyRequestRule(Tableau *tableau, shared_ptr<Tableau::Node> node)
{
    Tableau::Node *currentNode = &(*node);
    while (currentNode != nullptr)
    {
        if (currentNode->relation != nullptr && currentNode->relation->negated)
        {
            // hack: make shared node to only check the new mteastatement and not all again
            auto rNegA = negARule(make_shared<Tableau::Node>(tableau, node->metastatement), currentNode->relation);
            if (rNegA)
            {
                (*rNegA)->negated = currentNode->relation->negated;
                node->appendBranches(*rNegA);
                // return; // TODO: hack, do not return allow multiple applications of metastatement
            }
        }
        currentNode = currentNode->parentNode;
    }
}

bool Tableau::applyRule(shared_ptr<Tableau::Node> node)
{
    if (node->metastatement != nullptr)
    {
        applyRequestRule(this, node);
        // no rule applicable anymore (metastatement)
        return false; // TODO: this value may be true
    }
    if (applyDNFRule(node))
    {
        return true;
    }
    // Rule::a, Rule::negA
    if (node->relation->negated)
    {
        auto rNegA = negARule(node, node->relation);
        if (rNegA)
        {
            (*rNegA)->negated = node->relation->negated;
            node->appendBranches(*rNegA);
            return true;
        }
    }
    else
    {
        auto rA = aRule(node->relation);
        if (rA)
        {
            const auto &[r1, metastatement] = *rA;
            r1->negated = node->relation->negated;
            node->appendBranches(r1);
            node->appendBranches(metastatement);
            return true;
        }
    }

    // "no rule applicable"
    /* TODO
    case Rule::empty:
    case Rule::propagation:
    case Rule::at:
    case Rule::negAt:
    case Rule::converseA:
    case Rule::negConverseA:*/
    return false;
}

bool Tableau::solve(int bound)
{
    while (!unreducedNodes.empty() && bound > 0)
    {
        bound--;
        auto currentNode = unreducedNodes.top();
        unreducedNodes.pop();
        applyRule(currentNode);
    }
}

// helper
vector<vector<shared_ptr<Relation>>> extractDNF(shared_ptr<Tableau::Node> node)
{
    if (node == nullptr || node->isClosed())
    {
        return {};
    }
    if (node->isLeaf())
    {
        if (node->relation != nullptr && node->relation->isNormal())
        {
            return {{node->relation}};
        }
        return {};
    }
    else
    {
        vector<vector<shared_ptr<Relation>>> left = extractDNF(node->leftNode);
        vector<vector<shared_ptr<Relation>>> right = extractDNF(node->rightNode);
        left.insert(left.end(), right.begin(), right.end());
        if (node->relation != nullptr && node->relation->isNormal())
        {
            for (vector<shared_ptr<Relation>> &clause : left)
            {
                clause.push_back(node->relation);
            }
        }
        return left;
    }
}

vector<vector<shared_ptr<Relation>>> Tableau::DNF()
{
    while (!unreducedNodes.empty())
    {
        auto currentNode = unreducedNodes.top();
        unreducedNodes.pop();
        applyDNFRule(currentNode);
    }

    // exportProof("dnf");

    return extractDNF(rootNode);
}

bool Tableau::applyModalRule()
{
    // TODO: hack using that all terms are reduced, s.t. only possible next rule is modal
    while (!unreducedNodes.empty())
    {
        auto currentNode = unreducedNodes.top();
        unreducedNodes.pop();
        if (applyRule(currentNode))
        {
            return true;
        }
    }
    return false;
}

vector<shared_ptr<Relation>> Tableau::calcReuqest()
{
    while (!unreducedNodes.empty())
    {
        auto currentNode = unreducedNodes.top();
        unreducedNodes.pop();
        if (applyRequestRule(this, currentNode))
        {
            break; // metastatement found and all requests calculated
        }
    }

    // exrtact terms for new node
    shared_ptr<Node> node = rootNode;
    shared_ptr<Relation> oldPositive = nullptr;
    shared_ptr<Relation> newPositive = nullptr;
    vector<int> activeLabels;
    while (node != nullptr) // exploit that only alpha rules applied
    {
        if (node->relation != nullptr && !node->relation->negated)
        {
            if (oldPositive == nullptr)
            {
                oldPositive = node->relation;
            }
            else
            {
                newPositive = node->relation;
                activeLabels = node->relation->labels();
            }
        }
        node = node->leftNode;
    }
    vector<shared_ptr<Relation>> request{newPositive};
    node = rootNode;
    while (node != nullptr) // exploit that only alpha rules applied
    {
        if (node->relation != nullptr && node->relation->negated)
        {
            vector<int> relationLabels = node->relation->labels();
            bool allLabelsActive = true;
            for (auto label : relationLabels)
            {
                if (find(activeLabels.begin(), activeLabels.end(), label) == activeLabels.end())
                {
                    allLabelsActive = false;
                    break;
                }
            }
            if (allLabelsActive)
            {
                request.push_back(node->relation);
            }
        }
        node = node->leftNode;
    }
    return request;
}

void Tableau::toDotFormat(ofstream &output) const
{
    output << "graph {" << endl
           << "node[shape=\"plaintext\"]" << endl;
    rootNode->toDotFormat(output);
    output << "}" << endl;
}

void Tableau::exportProof(string filename)
{
    ofstream file(filename + ".dot");
    toDotFormat(file);
}
