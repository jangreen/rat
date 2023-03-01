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

Tableau::Node::Node(shared_ptr<Relation> relation) : relation(relation) {}
Tableau::Node::Node(shared_ptr<Metastatement> metastatement) : metastatement(metastatement) {}
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

void Tableau::Node::appendBranches(shared_ptr<Node> leftNode, shared_ptr<Node> rightNode)
{
    // do not expand closed branches
    if (isClosed())
    {
        return;
    }
    if (isLeaf())
    {
        this->leftNode = leftNode;
        leftNode->parentNode = this;
        if (rightNode != nullptr)
        {
            this->rightNode = rightNode;
            rightNode->parentNode = this;
        }
        // check if closed
        if (leftNode->relation != nullptr)
        {
            Node *currentNode = this;
            while (currentNode != nullptr)
            {
                if (currentNode->relation != nullptr && *currentNode->relation == *leftNode->relation && currentNode->relation->negated != leftNode->relation->negated)
                {
                    leftNode->closed = true;
                    currentNode = nullptr;
                }
                else
                {
                    currentNode = currentNode->parentNode;
                }
            }
        }

        if (rightNode != nullptr && rightNode->relation != nullptr)
        {
            Node *currentNode = this;
            while (currentNode != nullptr)
            {
                if (currentNode->relation != nullptr && *currentNode->relation == *rightNode->relation && currentNode->relation->negated != rightNode->relation->negated)
                {
                    rightNode->closed = true;
                    currentNode = nullptr;
                }
                else
                {
                    currentNode = currentNode->parentNode;
                }
            }
        }

        return;
    }
    if (this->leftNode != nullptr)
    {
        this->leftNode->appendBranches(leftNode, rightNode);
    }
    if (this->rightNode != nullptr)
    {
        this->rightNode->appendBranches(leftNode, rightNode);
    }
}

void Tableau::Node::toDotFormat(ofstream &output) const
{
    output << "N" << this
           << "[label=\"";
    if (relation != nullptr)
    {
        output << (relation->negated ? "-. " : "")
               << relation->toString();
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

Tableau::Tableau(initializer_list<shared_ptr<Relation>> initalRelations) : Tableau(unordered_set(initalRelations)) {}
Tableau::Tableau(unordered_set<shared_ptr<Relation>> initalRelations)
{
    shared_ptr<Node> currentNode = nullptr;
    for (auto relation : initalRelations)
    {
        shared_ptr<Node> newNode = make_shared<Node>(relation);
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

void Tableau::applyRule(shared_ptr<Tableau::Node> node)
{
    if (node->metastatement != nullptr)
    {
        Tableau::Node *currentNode = &(*node);
        while (currentNode != nullptr)
        {
            if (currentNode->relation != nullptr && currentNode->relation->negated)
            {
                // hack: make shared node to only check the new mteastatement and not all again
                auto rNegA = negARule(make_shared<Node>(node->metastatement), currentNode->relation);
                if (rNegA)
                {
                    cout << "(-.a)" << endl;
                    (*rNegA)->negated = currentNode->relation->negated;
                    shared_ptr<Tableau::Node> newNode1 = make_shared<Tableau::Node>(*rNegA);
                    node->appendBranches(newNode1);
                    unreducedNodes.push(newNode1);
                    // return; // TODO: hack, do not return allow multiple applications of metastatement
                }
            }
            currentNode = currentNode->parentNode;
        }

        cout << "no rule applicable (metastatement)" << endl;
        return;
    }
    // Rule::id, Rule::negId
    auto rId = idRule(node->relation);
    if (rId)
    {
        cout << "(id)" << endl;
        (*rId)->negated = node->relation->negated;
        shared_ptr<Tableau::Node> newNode1 = make_shared<Tableau::Node>(*rId);
        node->appendBranches(newNode1);
        unreducedNodes.push(newNode1);
        return;
    }
    // Rule::composition, Rule::negComposition
    auto rComposition = compositionRule(node->relation);
    if (rComposition)
    {
        cout << "(;)" << endl;
        (*rComposition)->negated = node->relation->negated;
        shared_ptr<Tableau::Node> newNode1 = make_shared<Tableau::Node>(*rComposition);
        node->appendBranches(newNode1);
        unreducedNodes.push(newNode1);
        return;
    }
    // Rule::intersection, Rule::negIntersection
    auto rIntersection = intersectionRule(node->relation);
    if (rIntersection)
    {
        cout << "(&)" << endl;
        (*rIntersection)->negated = node->relation->negated;
        shared_ptr<Tableau::Node> newNode1 = make_shared<Tableau::Node>(*rIntersection);
        node->appendBranches(newNode1);
        unreducedNodes.push(newNode1);
        return;
    }
    // Rule::choice, Rule::negChoice
    auto rChoice = choiceRule(node->relation);
    if (rChoice)
    {
        cout << "(|)" << endl;
        const auto &[r1, r2] = *rChoice;
        r1->negated = node->relation->negated;
        r2->negated = node->relation->negated;
        shared_ptr<Tableau::Node> newNode1 = make_shared<Tableau::Node>(r1);
        shared_ptr<Tableau::Node> newNode2 = make_shared<Tableau::Node>(r2);
        if (node->relation->negated)
        {
            node->appendBranches(newNode1);
            node->appendBranches(newNode2);
        }
        else
        {
            node->appendBranches(newNode1, newNode2);
        }
        unreducedNodes.push(newNode1);
        unreducedNodes.push(newNode2);
        return;
    }
    // Rule::transitiveClosure, Rule::negTransitiveClosure
    auto rTransitiveClosure = transitiveClosureRule(node->relation);
    if (rTransitiveClosure)
    {
        cout << "(*)" << endl;
        const auto &[r1, r2] = *rTransitiveClosure;
        r1->negated = node->relation->negated;
        r2->negated = node->relation->negated;
        shared_ptr<Tableau::Node> newNode1 = make_shared<Tableau::Node>(r1);
        shared_ptr<Tableau::Node> newNode2 = make_shared<Tableau::Node>(r2);
        if (node->relation->negated)
        {
            node->appendBranches(newNode1);
            node->appendBranches(newNode2);
        }
        else
        {
            node->appendBranches(newNode1, newNode2);
        }
        unreducedNodes.push(newNode1);
        unreducedNodes.push(newNode2);
        return;
    }
    // Rule::a, Rule::negA
    if (node->relation->negated)
    {
        auto rNegA = negARule(node, node->relation);
        if (rNegA)
        {
            cout << "(-.a)" << endl;
            (*rNegA)->negated = node->relation->negated;
            shared_ptr<Tableau::Node> newNode1 = make_shared<Tableau::Node>(*rNegA);
            node->appendBranches(newNode1);
            unreducedNodes.push(newNode1);
            return;
        }
    }
    else
    {
        auto rA = aRule(node->relation);
        if (rA)
        {
            cout << "(a)" << endl;
            const auto &[r1, metastatement] = *rA;
            r1->negated = node->relation->negated;
            shared_ptr<Tableau::Node> newNode1 = make_shared<Tableau::Node>(r1);
            shared_ptr<Tableau::Node> newNode2 = make_shared<Tableau::Node>(metastatement);
            node->appendBranches(newNode1);
            node->appendBranches(newNode2);
            unreducedNodes.push(newNode1);
            unreducedNodes.push(newNode2);
            return;
        }
    }

    cout << "no rule applicable" << endl;
    /* TODO
    case Rule::empty:
    case Rule::propagation:
    case Rule::at:
    case Rule::negAt:
    case Rule::converseA:
    case Rule::negConverseA:*/
}

bool Tableau::solve(int bound)
{
    while (!unreducedNodes.empty() && bound > 0)
    {
        bound--;
        auto currentNode = unreducedNodes.top();
        unreducedNodes.pop();
        if (currentNode->relation != nullptr)
        {
            cout << "Current node: " << currentNode->relation->toString() << endl;
        }
        else
        {
            cout << "Current node: " << currentNode->metastatement->toString() << endl;
        }

        /* TODO: remove, usful for debugging
        ofstream file("output.dot");
        toDotFormat(file);
        cin.get(); //*/
        applyRule(currentNode);
    }
}

void Tableau::toDotFormat(ofstream &output) const
{
    output << "graph {" << endl
           << "node[shape=\"plaintext\"]" << endl;
    rootNode->toDotFormat(output);
    output << "}" << endl;
}