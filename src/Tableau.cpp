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
                shared_ptr<Relation> r1 = make_shared<Relation>(*relation->rightOperand); // empty relation
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
                shared_ptr<Relation> r1 = make_shared<Relation>(*relation->rightOperand); // empty relation
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
        // Rule::a, Rule::negA
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

Tableau::Node::Node(bool negated, shared_ptr<Relation> relation) : negated(negated), relation(relation) {}
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
    cout << 111 << endl;
    if (isLeaf())
    {
        cout << 112 << endl;
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
            cout << 115 << endl;
            while (currentNode != nullptr)
            {
                if (currentNode->relation != nullptr && *currentNode->relation == *leftNode->relation && currentNode->negated != leftNode->negated)
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

        cout << 114 << endl;
        if (rightNode != nullptr && rightNode->relation != nullptr)
        {
            Node *currentNode = this;
            while (currentNode != nullptr)
            {
                if (currentNode->relation != nullptr && *currentNode->relation == *rightNode->relation && currentNode->negated != rightNode->negated)
                {
                    rightNode->closed = true;
                    currentNode = nullptr;
                }
                else
                {
                    currentNode = currentNode->parentNode;
                }
            }
            cout << 113 << endl;
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
        output << (negated ? "-. " : "")
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
        if (leftNode->parentNode != this) // TODO: remove
        {
            cout << "Error " << leftNode->parentNode << " . " << this << endl;
        };
    }
    if (rightNode != nullptr)
    {
        rightNode->toDotFormat(output);
        output << "N" << this << " -- "
               << "N" << rightNode << ";" << endl;
        if (rightNode->parentNode != this) // TODO: remove
        {
            cout << "Error" << endl;
        };
    }
}

Tableau::Tableau(initializer_list<shared_ptr<Node>> initalNodes)
{
    rootNode = *initalNodes.begin();
    auto currentNode = rootNode;
    for (auto node : initalNodes)
    {
        currentNode->leftNode = node;
        node->parentNode = &(*currentNode);
        currentNode = node;
        unreducedNodes.push(node);
    }
    rootNode->parentNode = nullptr;
}
Tableau::~Tableau() {}

void Tableau::applyRule(shared_ptr<Tableau::Node> node)
{
    // Rule::id, Rule::negId
    auto rId = idRule(node->relation);
    if (rId)
    {
        cout << "(id)" << endl;
        shared_ptr<Tableau::Node> newNode1 = make_shared<Tableau::Node>(node->negated, *rId);
        node->appendBranches(newNode1);
        unreducedNodes.push(newNode1);
        return;
    }
    // Rule::composition, Rule::negComposition
    auto rComposition = compositionRule(node->relation);
    if (rComposition)
    {
        cout << "(;)" << endl;
        shared_ptr<Tableau::Node> newNode1 = make_shared<Tableau::Node>(node->negated, *rComposition);
        node->appendBranches(newNode1);
        unreducedNodes.push(newNode1);
        return;
    }
    // Rule::intersection, Rule::negIntersection
    auto rIntersection = intersectionRule(node->relation);
    if (rIntersection)
    {
        cout << "(&)" << endl;
        shared_ptr<Tableau::Node> newNode1 = make_shared<Tableau::Node>(node->negated, *rIntersection);
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
        shared_ptr<Tableau::Node> newNode1 = make_shared<Tableau::Node>(node->negated, r1);
        shared_ptr<Tableau::Node> newNode2 = make_shared<Tableau::Node>(node->negated, r2);
        if (node->negated)
        {
            cout << 12 << endl;
            node->appendBranches(newNode1);
            cout << 13 << endl;
            node->appendBranches(newNode2);
            cout << 14 << endl;
        }
        else
        {
            node->appendBranches(newNode1, newNode2);
        }
        unreducedNodes.push(newNode1);
        unreducedNodes.push(newNode2);
        cout << 1 << endl;
        return;
    }
    cout << 2 << endl;
    // Rule::transitiveClosure, Rule::negTransitiveClosure
    auto rTransitiveClosure = transitiveClosureRule(node->relation);
    if (rTransitiveClosure)
    {
        cout << "(*)" << endl;
        const auto &[r1, r2] = *rTransitiveClosure;
        shared_ptr<Tableau::Node> newNode1 = make_shared<Tableau::Node>(node->negated, r1);
        shared_ptr<Tableau::Node> newNode2 = make_shared<Tableau::Node>(node->negated, r2);
        if (node->negated)
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
    auto rA = aRule(node->relation);
    if (rA && !node->negated)
    {
        cout << "(a)" << endl;
        const auto &[r1, metastatement] = *rA;
        shared_ptr<Tableau::Node> newNode1 = make_shared<Tableau::Node>(node->negated, r1);
        shared_ptr<Tableau::Node> newNode2 = make_shared<Tableau::Node>(metastatement);
        node->appendBranches(newNode1);
        node->appendBranches(newNode2);
        unreducedNodes.push(newNode1);
        // unreducedNodes.push(newNode2); // TODO push metastatements also on stack?
        return;
    }

    cout << "no rule applicable" << endl;
    /* TODO
    case Rule::empty:
    case Rule::bottom:
    case Rule::propagation:
    case Rule::at:
    case Rule::negAt:
    case Rule::negA:
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
        cout << "Current node: " << currentNode->relation->toString() << endl;
        /* TODO: remove, usful for debugging
        ofstream file("test.dot");
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