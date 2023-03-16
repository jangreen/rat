#include <iostream>
#include "Tableau.h"
#include "ProofRule.h"
#include "Metastatement.h"

using namespace std;

/* RULE IMPLEMENTATIONS */
/*template <typename T>
optional<T> rule(const Relation &relation, T (*labeledCase)(const Relation &), T (*combineLeft)(T, Relation), T (*combineRight)(T, Relation))
{
    if (relation.label)
    {
        return labeledCase(relation);
    }
    // case: intersection or composition (only cases for labeled terms that can happen)
    optional<T> leftSubRelation = rule<T>(relation.leftOperand, labeledCase, combineLeft);
    if (leftSubRelation)
    {
        return combineLeft(*leftSubRelation, relation);
    }
    // case: intersection
    if (relation.operation != Operation::intersection)
    {
        return nullopt;
    }
    optional<T> rightSubRelation = rule<T>(relation.rightOperand, labeledCase, combineLeft);
    if (rightSubRelation)
    {
        return combineRight(*rightSubRelation, relation);
    }
    return nullopt;
}
// TODO generice function: mayb binaryRule, ...
*/
optional<tuple<Relation, Relation>> choiceRule(const Relation &relation)
{
    if (!relation.label)
    {
        // case: intersection or composition (only cases for labeled terms that can happen)
        auto left = choiceRule(*relation.leftOperand);
        if (left)
        {
            auto &[subrelation1, subrelation2] = *left;
            tuple<Relation, Relation> result{
                Relation(relation.operation, move(subrelation1), Relation(*relation.rightOperand)),
                Relation(relation.operation, move(subrelation2), Relation(*relation.rightOperand))};
            return result;
        }

        // only intersection
        if (relation.operation != Operation::intersection)
        {
            return nullopt;
        }
        auto right = choiceRule(*relation.rightOperand);
        if (right)
        {
            auto &[subrelation1, subrelation2] = *right;
            tuple<Relation, Relation> result{
                Relation(relation.operation, Relation(*relation.leftOperand), move(subrelation1)),
                Relation(relation.operation, Relation(*relation.leftOperand), move(subrelation2))};
            return result;
        }
        return nullopt;
    }
    else if (relation.operation == Operation::choice)
    {
        // Rule::choice, Rule::negChoice
        Relation r1 = Relation(*relation.leftOperand);
        r1.label = relation.label;
        Relation r2 = Relation(*relation.rightOperand);
        r2.label = relation.label;
        tuple<Relation, Relation> result{move(r1), move(r2)};
        return result;
    }
    return nullopt;
}

optional<tuple<Relation, Relation>> transitiveClosureRule(const Relation &relation)
{
    if (!relation.label)
    {
        // case: intersection or composition (only cases for labeled terms that can happen)
        auto left = transitiveClosureRule(*relation.leftOperand);
        if (left)
        {
            auto &[subrelation1, subrelation2] = *left;
            tuple<Relation, Relation> result{
                Relation(relation.operation, move(subrelation1), Relation(*relation.rightOperand)),
                Relation(relation.operation, move(subrelation2), Relation(*relation.rightOperand))};
            return result;
        }

        // only intersection
        if (relation.operation != Operation::intersection)
        {
            return nullopt;
        }
        auto right = transitiveClosureRule(*relation.rightOperand);
        if (right)
        {
            auto &[subrelation1, subrelation2] = *right;
            tuple<Relation, Relation> result{
                Relation(relation.operation, Relation(*relation.leftOperand), move(subrelation1)),
                Relation(relation.operation, Relation(*relation.leftOperand), move(subrelation2))};
            return result;
        }
        return nullopt;
    }
    else if (relation.operation == Operation::transitiveClosure)
    {
        // Rule::transitiveClosure, Rule::negTransitiveClosure
        Relation r1 = Relation(*relation.leftOperand);
        r1.label = relation.label;
        Relation r12 = Relation(relation);
        r12.label = nullopt;
        r12.negated = false;
        Relation r2 = Relation(Operation::none); // empty relation
        r2.label = relation.label;
        tuple<Relation, Relation>
            result{
                Relation(Operation::composition, move(r1), move(r12)),
                move(r2)};
        return result;
    }
    return nullopt;
}

optional<Relation> idRule(const Relation &relation)
{
    if (!relation.label)
    {
        // case: intersection or composition (only cases for labeled terms that can happen)
        auto left = idRule(Relation(*relation.leftOperand));
        if (left)
        {
            // case: composition && emtpy labeled relation -> move label to 'next' relation
            if (relation.operation == Operation::composition && (*left).operation == Operation::none && !(*left).identifier && (*left).label)
            {
                Relation r1 = Relation(*relation.rightOperand);
                r1.label = (*left).label;
                return r1;
            }
            return Relation(relation.operation, move(*left), Relation(*relation.rightOperand));
        }
        // only intersection
        if (relation.operation != Operation::intersection)
        {
            return nullopt;
        }
        auto right = idRule(*relation.rightOperand);
        if (right)
        {
            return Relation(relation.operation, Relation(*relation.leftOperand), move(*right));
        }
        return nullopt;
    }
    else if (relation.operation == Operation::identity) // TODO compare relations with overloaded == operator
    {
        // Rule::id, Rule::negId
        Relation r1 = Relation(Operation::none); // empty relation
        r1.label = relation.label;
        return r1;
    }
    return nullopt;
}

optional<tuple<Relation, shared_ptr<Metastatement>>> aRule(const Relation &relation)
{
    if (!relation.label)
    {
        // case: intersection or composition (only cases for labeled terms that can happen)
        auto left = aRule(Relation(*relation.leftOperand));
        if (left)
        {
            auto &[subrelation1, metastatement] = *left;

            // case: composition && emtpy labeled relation -> move label to 'next' relation
            if (relation.operation == Operation::composition && subrelation1.operation == Operation::none && !subrelation1.identifier && subrelation1.label)
            {
                Relation r1 = Relation(*relation.rightOperand);
                r1.label = subrelation1.label;
                tuple<Relation, shared_ptr<Metastatement>> result{
                    move(r1),
                    move(metastatement)};
                return result;
            }
            tuple<Relation, shared_ptr<Metastatement>> result{
                Relation(relation.operation, move(subrelation1), Relation(*relation.rightOperand)),
                move(metastatement)};
            return result;
        }

        // only intersection
        if (relation.operation != Operation::intersection)
        {
            return nullopt;
        }
        auto right = aRule(*relation.rightOperand);
        if (right)
        {
            auto &[subrelation1, metastatement] = *right;
            tuple<Relation, shared_ptr<Metastatement>> result{
                Relation(relation.operation, Relation(*relation.leftOperand), move(subrelation1)),
                move(metastatement)};
            return result;
        }
        return nullopt;
    }
    else if (relation.operation == Operation::base) // TODO compare relations with overloaded == operator
    {
        // Rule::a
        Relation r1 = Relation(Operation::none); // empty relation
        Relation::maxLabel++;
        r1.label = Relation::maxLabel;
        tuple<Relation, shared_ptr<Metastatement>>
            result{
                move(r1),
                make_unique<Metastatement>(MetastatementType::labelRelation, *relation.label, Relation::maxLabel, *relation.identifier)};
        return result;
    }
    return nullopt;
}

optional<Relation> negARule(shared_ptr<Tableau::Node> node, const Relation &relation)
{
    if (!relation.label)
    {
        // case: intersection or composition (only cases for labeled terms that can happen)
        auto left = negARule(node, Relation(*relation.leftOperand));
        if (left)
        {
            // case: composition && emtpy labeled relation -> move label to 'next' relation
            if (relation.operation == Operation::composition && (*left).operation == Operation::none && !(*left).identifier && (*left).label)
            {
                Relation r1 = Relation(*relation.rightOperand);
                r1.label = (*left).label;
                return r1;
            }
            return Relation(relation.operation, move(*left), Relation(*relation.rightOperand));
        }
        // only intersection
        if (relation.operation != Operation::intersection)
        {
            return nullopt;
        }
        auto right = negARule(node, *relation.rightOperand);
        if (right)
        {
            return Relation(relation.operation, Relation(*relation.leftOperand), move(*right));
        }
        return nullopt;
    }
    else if (relation.operation == Operation::base) // TODO compare relations with overloaded == operator
    {
        // Rule::negA
        Relation r1 = Relation(Operation::none); // empty relation
        int label = *relation.label;
        string baseRelation = *relation.identifier;
        Tableau::Node *currentNode = &(*node);
        while (currentNode != nullptr)
        {
            if (currentNode->metastatement && currentNode->metastatement->type == MetastatementType::labelRelation)
            {
                if (currentNode->metastatement->label1 == label && currentNode->metastatement->baseRelation == baseRelation)
                {
                    r1.label = currentNode->metastatement->label2;
                    return r1;
                }
            }
            currentNode = currentNode->parentNode;
        }
    }
    return nullopt;
}

optional<Relation> compositionRule(const Relation &relation)
{
    if (!relation.label)
    {
        // case: intersection or composition (only cases for labeled terms that can happen)
        auto left = compositionRule(Relation(*relation.leftOperand));
        if (left)
        {
            return Relation(relation.operation, move(*left), Relation(*relation.rightOperand));
        }
        // only intersection
        if (relation.operation != Operation::intersection)
        {
            return nullopt;
        }
        auto right = compositionRule(*relation.rightOperand);
        if (right)
        {
            return Relation(relation.operation, Relation(*relation.leftOperand), move(*right));
        }
        return nullopt;
    }
    else if (relation.operation == Operation::composition)
    {
        // Rule::comnposition, Rule::negComposition
        Relation r1 = Relation(*relation.leftOperand);
        r1.label = relation.label;
        Relation r2 = Relation(*relation.rightOperand);
        return Relation(Operation::composition, move(r1), move(r2));
    }
    return nullopt;
}

optional<Relation> intersectionRule(const Relation &relation)
{
    if (!relation.label)
    {
        // case: intersection or composition (only cases for labeled terms that can happen)
        auto left = intersectionRule(*relation.leftOperand);
        if (left)
        {
            return Relation(relation.operation, move(*left), Relation(*relation.rightOperand));
        }
        // only intersection
        if (relation.operation != Operation::intersection)
        {
            return nullopt;
        }
        auto right = intersectionRule(*relation.rightOperand);
        if (right)
        {
            return Relation(relation.operation, Relation(*relation.leftOperand), move(*right));
        }
        return nullopt;
    }
    else if (relation.operation == Operation::intersection)
    {
        // Rule::inertsection, Rule::negIntersection
        Relation r1 = Relation(*relation.leftOperand);
        r1.label = relation.label;
        Relation r2 = Relation(*relation.rightOperand);
        r1.label = relation.label;
        return Relation(Operation::intersection, move(r1), move(r2));
    }
    return nullopt;
}

/* implemented in consitency check when term is added to branch
optional<Relation> negTopRule(const Relation & relation)
{
    if (relation.label && relation.negated && relation.operation == Operation::full)
    {
        // special case: rule only for trailing top occurences

    }
    return nullopt;
}*/
/* END RULE IMPLEMENTATIONS */

Tableau::Node::Node(Tableau *tableau, const Relation &&relation) : tableau(tableau), relation(relation) {}
Tableau::Node::Node(Tableau *tableau, const Metastatement &&metastatement) : tableau(tableau), metastatement(metastatement) {}

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
bool checkIfClosed(Tableau::Node *node, const Relation &relation)
{
    while (node != nullptr)
    {
        if (node->relation && *node->relation == relation && node->relation->negated != relation.negated)
        {
            return true;
        }
        node = node->parentNode;
    }
    return false;
}

void Tableau::Node::appendBranches(const Relation &leftRelation)
{
    // do not expand closed branches
    if (isClosed())
    {
        return;
    }
    if (isLeaf())
    {
        auto leftNode = make_shared<Node>(tableau, Relation(leftRelation));
        this->leftNode = leftNode;
        leftNode->parentNode = this;
        leftNode->closed = checkIfClosed(this, leftRelation);
        if (leftRelation.negated && leftRelation.operation == Operation::full)
        {
            leftNode->closed = true;
        }
        if (!leftNode->closed)
        {
            tableau->unreducedNodes.push(leftNode);
        }

        return;
    }
    if (this->leftNode != nullptr)
    {
        this->leftNode->appendBranches(leftRelation);
    }
    if (this->rightNode != nullptr)
    {
        this->rightNode->appendBranches(leftRelation);
    }
}

void Tableau::Node::appendBranches(const Relation &leftRelation, const Relation &rightRelation)
{
    // do not expand closed branches
    if (isClosed())
    {
        return;
    }
    if (isLeaf())
    {
        auto leftNode = make_shared<Node>(tableau, Relation(leftRelation));
        this->leftNode = leftNode;
        leftNode->parentNode = this;
        leftNode->closed = checkIfClosed(this, leftRelation);
        if (leftRelation.negated && leftRelation.operation == Operation::full)
        {
            leftNode->closed = true;
        }
        if (!leftNode->closed)
        {
            tableau->unreducedNodes.push(leftNode);
        }

        auto rightNode = make_shared<Node>(tableau, Relation(rightRelation));
        this->rightNode = rightNode;
        rightNode->parentNode = this;
        rightNode->closed = checkIfClosed(this, rightRelation);
        if (rightRelation.negated && rightRelation.operation == Operation::full)
        {
            rightNode->closed = true;
        }
        if (!rightNode->closed)
        {
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
void Tableau::Node::appendBranches(const Metastatement &metastatement)
{
    // do not expand closed branches
    if (isClosed())
    {
        return;
    }
    if (isLeaf())
    {
        auto leftNode = make_shared<Node>(tableau, Metastatement(metastatement));
        this->leftNode = leftNode;
        leftNode->parentNode = this;
        tableau->unreducedNodes.push(leftNode);
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
    if (relation)
    {
        output << (*relation).toString();
    }
    else
    {
        output << (*metastatement).toString();
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
    if (left->relation && right->relation)
    {
        return left->relation->negated < right->relation->negated;
    }
    else if (right->relation)
    {
        return (!!left->metastatement) < right->relation->negated;
    }
    return true;
}

Tableau::Tableau(initializer_list<Relation> initalRelations) : Tableau(vector(initalRelations)) {}
Tableau::Tableau(Clause initalRelations)
{
    shared_ptr<Node> currentNode = nullptr;
    for (auto relation : initalRelations)
    {
        shared_ptr<Node> newNode = make_shared<Node>(this, Relation(relation));
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

// helper
bool applyDNFRule(shared_ptr<Tableau::Node> node)
{
    // Rule::id, Rule::negId
    auto rId = idRule(*node->relation);
    if (rId)
    {
        (*rId).negated = node->relation->negated;
        node->appendBranches(*rId);
        return true;
    }
    // Rule::composition, Rule::negComposition
    auto rComposition = compositionRule(*node->relation);
    if (rComposition)
    {
        (*rComposition).negated = node->relation->negated;
        node->appendBranches(*rComposition);
        return true;
    }
    // Rule::intersection, Rule::negIntersection
    auto rIntersection = intersectionRule(*node->relation);
    if (rIntersection)
    {
        (*rIntersection).negated = node->relation->negated;
        node->appendBranches(*rIntersection);
        return true;
    }
    // Rule::choice, Rule::negChoice
    auto rChoice = choiceRule(*node->relation);
    if (rChoice)
    {
        auto &[r1, r2] = *rChoice;
        r1.negated = (*node->relation).negated;
        r2.negated = (*node->relation).negated;
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
    auto rTransitiveClosure = transitiveClosureRule(*node->relation);
    if (rTransitiveClosure)
    {
        auto &[r1, r2] = *rTransitiveClosure;
        r1.negated = (*node->relation).negated;
        r2.negated = (*node->relation).negated;
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
        if (currentNode->relation && (*currentNode->relation).negated)
        {
            // hack: make shared node to only check the new mteastatement and not all again
            auto rNegA = negARule(make_shared<Tableau::Node>(tableau, Metastatement(*node->metastatement)), Relation(*currentNode->relation));
            if (rNegA)
            {
                (*rNegA).negated = (*currentNode->relation).negated;
                node->appendBranches(*rNegA);
                // return; // TODO: hack, do not return allow multiple applications of metastatement
            }
        }
        currentNode = currentNode->parentNode;
    }
}

bool Tableau::applyRule(shared_ptr<Tableau::Node> node)
{
    if (node->metastatement)
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
        auto rNegA = negARule(node, *node->relation);
        if (rNegA)
        {
            (*rNegA).negated = (*node->relation).negated;
            node->appendBranches(*rNegA);
            return true;
        }
    }
    else
    {
        auto rA = aRule(*node->relation);
        if (rA)
        {
            auto &[r1, metastatement] = *rA;
            r1.negated = (*node->relation).negated;
            node->appendBranches(r1);
            node->appendBranches(*metastatement);
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
        exportProof("inf");
        applyRule(currentNode);
    }
}

// helper
vector<Clause> extractDNF(shared_ptr<Tableau::Node> node)
{
    if (node == nullptr || node->isClosed())
    {
        return {};
    }
    if (node->isLeaf())
    {
        if (node->relation && node->relation->isNormal())
        {
            return {{*node->relation}};
        }
        return {};
    }
    else
    {
        vector<Clause> left = extractDNF(node->leftNode);
        vector<Clause> right = extractDNF(node->rightNode);
        left.insert(left.end(), right.begin(), right.end());
        if (node->relation && node->relation->isNormal())
        {
            for (Clause &clause : left)
            {
                clause.push_back(*node->relation);
            }
        }
        return left;
    }
}

vector<Clause> Tableau::DNF()
{
    while (!unreducedNodes.empty())
    {
        auto currentNode = unreducedNodes.top();
        unreducedNodes.pop();
        exportProof("dnfcalc");
        applyDNFRule(currentNode);
    }

    exportProof("dnfcalc");

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

Clause Tableau::calcReuqest()
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
    optional<Relation> oldPositive;
    optional<Relation> newPositive;
    vector<int> activeLabels;
    while (node != nullptr) // exploit that only alpha rules applied
    {
        if (node->relation && !node->relation->negated)
        {
            if (!oldPositive)
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
    Clause request{*newPositive};
    node = rootNode;
    while (node != nullptr) // exploit that only alpha rules applied
    {
        if (node->relation && node->relation->negated)
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
                request.push_back(*node->relation);
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

void Tableau::exportProof(string filename) const
{
    ofstream file(filename + ".dot");
    toDotFormat(file);
}
