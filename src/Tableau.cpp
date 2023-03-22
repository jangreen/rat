#include <iostream>
#include "Tableau.h"
#include "ProofRule.h"
#include "Metastatement.h"

/* RULE IMPLEMENTATIONS */
template <typename T>
std::optional<T> rule(const Relation &relation, auto &baseCase, auto &combineLeft, auto &combineRight)
{
    if (relation.label)
    {
        return baseCase(relation);
    }
    // case: intersection or composition (only cases for labeled terms that can happen)
    std::optional<T> leftSubRelation = rule<T>(*relation.leftOperand, baseCase, combineLeft, combineRight);
    if (leftSubRelation)
    {
        return combineLeft(*leftSubRelation, relation);
    }
    // case: intersection
    if (relation.operation != Operation::intersection)
    {
        return std::nullopt;
    }
    std::optional<T> rightSubRelation = rule<T>(*relation.rightOperand, baseCase, combineLeft, combineRight);
    if (rightSubRelation)
    {
        return combineRight(*rightSubRelation, relation);
    }
    return std::nullopt;
}

std::optional<std::tuple<Relation, Relation>> binaryRule(const Relation &relation, auto &baseCase)
{
    auto combineLeft = [](std::tuple<Relation, Relation> &subrelations, const Relation &relation)
    {
        auto &[subrelation1, subrelation2] = subrelations;
        if (relation.operation == Operation::composition && subrelation1.operation == Operation::none && subrelation1.label)
        {
            // TODO: is correct? check only for subrelation1?
            Relation r1(*relation.rightOperand);
            r1.label = subrelation1.label;
            std::tuple<Relation, Relation> result{
                r1,
                Relation(relation.operation, std::move(subrelation2), Relation(*relation.rightOperand))};
            return result;
        }
        std::tuple<Relation, Relation> result{
            Relation(relation.operation, std::move(subrelation1), Relation(*relation.rightOperand)),
            Relation(relation.operation, std::move(subrelation2), Relation(*relation.rightOperand))};
        return result;
    };
    auto combineRight = [](std::tuple<Relation, Relation> &subrelations, const Relation &relation)
    {
        auto &[subrelation1, subrelation2] = subrelations;
        std::tuple<Relation, Relation> result{
            Relation(relation.operation, Relation(*relation.leftOperand), std::move(subrelation1)),
            Relation(relation.operation, Relation(*relation.leftOperand), std::move(subrelation2))};
        return result;
    };
    return rule<std::tuple<Relation, Relation>>(relation, baseCase, combineLeft, combineRight);
}
std::optional<Relation> unaryRule(const Relation &relation, auto &baseCase)
{
    auto combineLeft = [](Relation &subrelation, const Relation &relation)
    {
        if (relation.operation == Operation::composition && subrelation.operation == Operation::none && subrelation.label)
        {
            Relation r1(*relation.rightOperand);
            r1.label = subrelation.label;
            return r1;
        }
        return Relation(relation.operation, std::move(subrelation), Relation(*relation.rightOperand));
    };
    auto combineRight = [](Relation &subrelation, const Relation &relation)
    {
        return Relation(relation.operation, Relation(*relation.leftOperand), std::move(subrelation));
    };
    return rule<Relation>(relation, baseCase, combineLeft, combineRight);
}

std::optional<std::tuple<Relation, std::shared_ptr<Metastatement>>> aRule(const Relation &relation)
{
    auto combineLeft = [](std::tuple<Relation, std::shared_ptr<Metastatement>> &subrelations, const Relation &relation)
    {
        auto &[subrelation1, metastatement] = subrelations;

        // case: composition && emtpy labeled relation -> move label to 'next' relation
        if (relation.operation == Operation::composition && subrelation1.operation == Operation::none && !subrelation1.identifier && subrelation1.label)
        {
            Relation r1(*relation.rightOperand);
            r1.label = subrelation1.label;
            std::tuple<Relation, std::shared_ptr<Metastatement>> result{
                std::move(r1),
                std::move(metastatement)};
            return result;
        }
        std::tuple<Relation, std::shared_ptr<Metastatement>> result{
            Relation(relation.operation, std::move(subrelation1), Relation(*relation.rightOperand)),
            std::move(metastatement)};
        return result;
    };
    auto combineRight = [](std::tuple<Relation, std::shared_ptr<Metastatement>> &subrelations, const Relation &relation)
    {
        auto &[subrelation1, metastatement] = subrelations;
        std::tuple<Relation, std::shared_ptr<Metastatement>> result{
            Relation(relation.operation, Relation(*relation.leftOperand), std::move(subrelation1)),
            std::move(metastatement)};
        return result;
    };
    auto baseCase = [](const Relation &relation) -> std::optional<std::tuple<Relation, std::shared_ptr<Metastatement>>>
    {
        if (relation.operation == Operation::base) // TODO compare relations with overloaded == operator
        {
            // Rule::a
            Relation r1(Operation::none); // empty relation
            Relation::maxLabel++;
            r1.label = Relation::maxLabel;
            std::tuple<Relation, std::shared_ptr<Metastatement>> result{
                std::move(r1),
                std::make_unique<Metastatement>(MetastatementType::labelRelation, *relation.label, Relation::maxLabel, *relation.identifier)};
            return result;
        }
        return std::nullopt;
    };
    return rule<std::tuple<Relation, std::shared_ptr<Metastatement>>>(relation, baseCase, combineLeft, combineRight);
}

std::optional<std::tuple<Relation, std::shared_ptr<Metastatement>>> atRule(const Relation &relation)
{
    if (relation.leftOperand != nullptr && relation.rightOperand != nullptr && relation.leftOperand->operation == Operation::none && relation.rightOperand->operation == Operation::none && relation.operation == Operation::intersection)
    {
        // identify labels
        Relation r1(Operation::none);
        r1.label = std::min(*relation.leftOperand->label, *relation.rightOperand->label);
        std::tuple<Relation, std::shared_ptr<Metastatement>> result{
            std::move(r1),
            std::make_unique<Metastatement>(MetastatementType::labelEquality, *relation.leftOperand->label, *relation.rightOperand->label)};
        return result;
    }
    if (relation.leftOperand != nullptr)
    {
        // case: intersection or composition (only cases for labeled terms that can happen)
        auto leftSubRelation = atRule(*relation.leftOperand);
        if (leftSubRelation)
        {
            auto &[subrelation1, metastatement] = *leftSubRelation;
            std::tuple<Relation, std::shared_ptr<Metastatement>> result{
                Relation(relation.operation, std::move(subrelation1), Relation(*relation.rightOperand)),
                std::move(metastatement)};
            return result;
        }
    }
    if (relation.rightOperand != nullptr)
    {
        // case: intersection
        if (relation.operation != Operation::intersection)
        {
            return std::nullopt;
        }
        auto rightSubRelation = atRule(*relation.rightOperand);
        if (rightSubRelation)
        {
            auto &[subrelation1, metastatement] = *rightSubRelation;
            std::tuple<Relation, std::shared_ptr<Metastatement>> result{
                Relation(relation.operation, Relation(*relation.leftOperand), std::move(subrelation1)),
                std::move(metastatement)};
            return result;
        }
        return std::nullopt;
    }
    return std::nullopt;
}
std::optional<Relation> negAtRule(const Relation &relation)
{
    if (relation.leftOperand != nullptr && relation.rightOperand != nullptr && relation.leftOperand->operation == Operation::none && relation.rightOperand->operation == Operation::none && relation.operation == Operation::intersection)
    {
        // identify labels
        if (*relation.leftOperand->label == *relation.rightOperand->label)
        {
            Relation r1(Operation::none);
            r1.label = *relation.leftOperand->label;
            return r1;
        }
        return std::nullopt;
    }
    // TODO DRY: use template functions
    // case: intersection or composition (only cases for labeled terms that can happen)
    if (relation.leftOperand != nullptr)
    {
        auto leftSubRelation = negAtRule(*relation.leftOperand);
        if (leftSubRelation)
        {
            if (relation.operation == Operation::composition && leftSubRelation->operation == Operation::none && leftSubRelation->label)
            {
                Relation r1(*relation.rightOperand);
                r1.label = leftSubRelation->label;
                return r1;
            }
            return Relation(relation.operation, std::move(*leftSubRelation), Relation(*relation.rightOperand));
        }
        return std::nullopt;
    }
    if (relation.rightOperand != nullptr)
    {
        // case: intersection
        if (relation.operation != Operation::intersection)
        {
            return std::nullopt;
        }
        auto rightSubRelation = negAtRule(*relation.rightOperand);
        if (rightSubRelation)
        {
            return Relation(relation.operation, Relation(*relation.leftOperand), std::move(*rightSubRelation));
        }
        return std::nullopt;
    }
    return std::nullopt;
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
    if (relation.negated && relation.operation == Operation::full)
    {
        return true;
    }

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
bool checkIfDuplicate(Tableau::Node *node, const Relation &relation)
{
    while (node != nullptr)
    {
        if (node->relation && *node->relation == relation)
        {
            return true;
        }
        node = node->parentNode;
    }
    return false;
}

// helper
std::optional<Tableau::Node> createNode(Tableau::Node *parent, const Relation &relation)
{
    // create new node by copying
    Tableau::Node newNode(parent->tableau, Relation(relation));
    newNode.closed = checkIfClosed(parent, relation);
    if (!newNode.closed && checkIfDuplicate(parent, relation))
    {
        return std::nullopt;
    }
    newNode.parentNode = parent;
    newNode.parentMetastatement = (parent->metastatement) ? parent : parent->parentMetastatement;
    return newNode;
}

void Tableau::Node::appendBranches(const Relation &leftRelation)
{
    if (!isClosed() && isLeaf())
    {
        auto newNode = createNode(this, leftRelation);
        if (newNode)
        {
            leftNode = std::make_shared<Node>(std::move(*newNode));
            if (!leftNode->closed)
            {
                tableau->unreducedNodes.push(leftNode);
            }
        }
    }
    else
    {
        if (leftNode != nullptr)
        {
            leftNode->appendBranches(leftRelation);
        }
        if (rightNode != nullptr)
        {
            rightNode->appendBranches(leftRelation);
        }
    }
}

void Tableau::Node::appendBranches(const Relation &leftRelation, const Relation &rightRelation)
{
    if (!isClosed() && isLeaf())
    {
        auto newNodeLeft = createNode(this, leftRelation);
        auto newNodeRight = createNode(this, rightRelation);
        if (newNodeLeft)
        {
            leftNode = std::make_shared<Node>(std::move(*newNodeLeft));
            if (!leftNode->closed)
            {
                tableau->unreducedNodes.push(leftNode);
            }
        }
        if (newNodeRight)
        {
            rightNode = std::make_shared<Node>(std::move(*newNodeRight));
            if (!rightNode->closed)
            {
                tableau->unreducedNodes.push(rightNode);
            }
        }
    }
    else
    {
        if (leftNode != nullptr)
        {
            leftNode->appendBranches(leftRelation, rightRelation);
        }
        if (rightNode != nullptr)
        {
            rightNode->appendBranches(leftRelation, rightRelation);
        }
    }
}
// TODO DRY
void Tableau::Node::appendBranches(const Metastatement &metastatement)
{
    if (!isClosed() && isLeaf())
    {
        leftNode = std::make_shared<Node>(tableau, Metastatement(metastatement));
        leftNode->parentNode = this;
        leftNode->parentMetastatement = (this->metastatement) ? this : parentMetastatement;
        tableau->unreducedNodes.push(leftNode);
    }
    else
    {
        if (leftNode != nullptr)
        {
            leftNode->appendBranches(metastatement);
        }
        if (rightNode != nullptr)
        {
            rightNode->appendBranches(metastatement);
        }
    }
}

void Tableau::Node::toDotFormat(std::ofstream &output) const
{
    output << "N" << this
           << "[label=\"";
    if (relation)
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
        << "];" << std::endl;
    // children
    if (leftNode != nullptr)
    {
        leftNode->toDotFormat(output);
        output << "N" << this << " -- "
               << "N" << leftNode << ";" << std::endl;
    }
    if (rightNode != nullptr)
    {
        rightNode->toDotFormat(output);
        output << "N" << this << " -- "
               << "N" << rightNode << ";" << std::endl;
    }
}

bool Tableau::Node::CompareNodes::operator()(const std::shared_ptr<Node> left, const std::shared_ptr<Node> right) const
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

Tableau::Tableau(std::initializer_list<Relation> initalRelations) : Tableau(std::vector(initalRelations)) {}
Tableau::Tableau(Clause initalRelations)
{
    std::shared_ptr<Node> currentNode = nullptr;
    for (auto relation : initalRelations)
    {
        std::shared_ptr<Node> newNode = std::make_shared<Node>(this, Relation(relation));
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
bool applyDNFRule(std::shared_ptr<Tableau::Node> node)
{
    // Rule::id, Rule::negId
    auto idRule = [](const Relation &relation) -> std::optional<Relation>
    {
        if (relation.operation == Operation::identity) // TODO compare relations with overloaded == operator
        {
            // Rule::id, Rule::negId
            Relation r1(Operation::none); // empty relation
            r1.label = relation.label;
            return r1;
        }
        return std::nullopt;
    };
    auto rId = unaryRule(*node->relation, idRule);
    if (rId)
    {
        rId->negated = node->relation->negated;
        node->appendBranches(*rId);
        return true;
    }
    // Rule::composition, Rule::negComposition
    auto compositionRule = [](const Relation &relation) -> std::optional<Relation>
    {
        if (relation.operation == Operation::composition)
        {
            // Rule::comnposition, Rule::negComposition
            Relation r1(*relation.leftOperand);
            r1.label = relation.label;
            Relation r2(*relation.rightOperand);
            return Relation(Operation::composition, std::move(r1), std::move(r2));
        }
        return std::nullopt;
    };
    auto rComposition = unaryRule(*node->relation, compositionRule);
    if (rComposition)
    {
        rComposition->negated = node->relation->negated;
        node->appendBranches(*rComposition);
        return true;
    }
    // Rule::intersection, Rule::negIntersection
    auto intersectionRule = [](const Relation &relation) -> std::optional<Relation>
    {
        if (relation.operation == Operation::intersection)
        {
            // Rule::inertsection, Rule::negIntersection
            Relation r1(*relation.leftOperand);
            r1.label = relation.label;
            Relation r2(*relation.rightOperand);
            r2.label = relation.label;
            return Relation(Operation::intersection, std::move(r1), std::move(r2));
        }
        return std::nullopt;
    };
    auto rIntersection = unaryRule(*node->relation, intersectionRule);
    if (rIntersection)
    {
        rIntersection->negated = node->relation->negated;
        node->appendBranches(*rIntersection);
        return true;
    }
    if (!node->relation->negated)
    {
        auto rAt = atRule(*node->relation);
        if (rAt)
        {
            auto &[r1, metastatement] = *rAt;
            r1.negated = node->relation->negated;
            node->appendBranches(r1);
            node->appendBranches(*metastatement);
            return true;
        }
    }
    else
    {
        auto rNegAt = negAtRule(*node->relation);
        if (rNegAt)
        {
            auto r1 = *rNegAt;
            r1.negated = node->relation->negated;
            node->appendBranches(r1);
            return true;
        }
    }
    // Rule::choice, Rule::negChoice
    auto choiceRule = [](const Relation &relation) -> std::optional<std::tuple<Relation, Relation>>
    {
        if (relation.operation == Operation::choice)
        {
            // Rule::choice, Rule::negChoice
            Relation r1(*relation.leftOperand);
            r1.label = relation.label;
            Relation r2(*relation.rightOperand);
            r2.label = relation.label;
            std::tuple<Relation, Relation> result{std::move(r1), std::move(r2)};
            return result;
        }
        return std::nullopt;
    };
    auto rChoice = binaryRule(*node->relation, choiceRule);
    if (rChoice)
    {
        auto &[r1, r2] = *rChoice;
        r1.negated = node->relation->negated;
        r2.negated = node->relation->negated;
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
    auto transitiveClosureRule = [](const Relation &relation) -> std::optional<std::tuple<Relation, Relation>>
    {
        if (relation.operation == Operation::transitiveClosure)
        {
            // Rule::transitiveClosure, Rule::negTransitiveClosure
            Relation r1(*relation.leftOperand);
            r1.label = relation.label;
            Relation r12(relation);
            r12.label = std::nullopt;
            r12.negated = false;
            Relation r2(Operation::none); // empty relation
            r2.label = relation.label;
            std::tuple<Relation, Relation>
                result{
                    std::move(r2),
                    Relation(Operation::composition, std::move(r1), std::move(r12))};
            return result;
        }
        return std::nullopt;
    };
    auto rTransitiveClosure = binaryRule(*node->relation, transitiveClosureRule);
    if (rTransitiveClosure)
    {
        auto &[r1, r2] = *rTransitiveClosure;
        r1.negated = node->relation->negated;
        r2.negated = node->relation->negated;
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
    return false;
}

// helper
bool applyRequestRule(Tableau *tableau, std::shared_ptr<Tableau::Node> node)
{
    Tableau::Node *currentNode = &(*node);
    while (currentNode != nullptr)
    {
        if (currentNode->relation && currentNode->relation->negated)
        {
            // hack: make shared node to only check the new mteastatement and not all again
            auto negARule = [node](const Relation &relation) -> std::optional<Relation>
            {
                if (relation.operation == Operation::base) // TODO compare relations with overloaded == operator
                {
                    // Rule::negA
                    Relation r1(Operation::none); // empty relation
                    int label = *relation.label;
                    std::string baseRelation = *relation.identifier;
                    if (node->metastatement && node->metastatement->type == MetastatementType::labelRelation)
                    {
                        if (node->metastatement->label1 == label && node->metastatement->baseRelation == baseRelation)
                        {
                            r1.label = node->metastatement->label2;
                            return r1;
                        }
                    }
                }
                return std::nullopt;
            };
            auto rNegA = unaryRule(*currentNode->relation, negARule);
            if (rNegA)
            {
                rNegA->negated = currentNode->relation->negated;
                node->appendBranches(*rNegA);
                // return; // TODO: hack, do not return allow multiple applications of metastatement
            }
        }
        currentNode = currentNode->parentNode;
    }
}
bool applyPropagationRule(Tableau *tableau, std::shared_ptr<Tableau::Node> node)
{
    Tableau::Node *currentNode = &(*node);
    while (currentNode != nullptr)
    {
        if (currentNode->relation && currentNode->relation->negated)
        {
            // hack: make shared node to only check the new mteastatement and not all again
            auto propRule = [node](const Relation &relation) -> std::optional<Relation>
            {
                int maxLabel = std::max(node->metastatement->label1, node->metastatement->label2);
                int minLabel = std::min(node->metastatement->label1, node->metastatement->label2);
                if (relation.label && *relation.label == maxLabel)
                {
                    // Rule::prop
                    Relation r1(relation); // empty relation
                    r1.label = minLabel;
                    return r1;
                }
                return std::nullopt;
            };
            auto rProp = unaryRule(*currentNode->relation, propRule);
            if (rProp)
            {
                rProp->negated = currentNode->relation->negated;
                node->appendBranches(*rProp);
                // return; // TODO: hack, do not return allow multiple applications of metastatement
            }
        }
        currentNode = currentNode->parentNode;
    }
}
bool applyRequestRuleNoMeta(Tableau *tableau, std::shared_ptr<Tableau::Node> node)
{
    Relation r = *node->relation;
    Tableau::Node *currentMetaNode = node->parentMetastatement;
    while (currentMetaNode != nullptr)
    {
        if (currentMetaNode->metastatement->type == MetastatementType::labelRelation)
        {
            // hack: make shared node to only check the new mteastatement and not all again
            auto negARule = [currentMetaNode](const Relation &relation) -> std::optional<Relation>
            {
                if (relation.operation == Operation::base)
                {
                    // Rule::negA
                    Relation r1(Operation::none); // empty relation
                    int label = *relation.label;
                    std::string baseRelation = *relation.identifier;
                    if (currentMetaNode->metastatement->label1 == label && currentMetaNode->metastatement->baseRelation == baseRelation)
                    {
                        r1.label = currentMetaNode->metastatement->label2;
                        return r1;
                    }
                }
                return std::nullopt;
            };
            auto rNegA = unaryRule(r, negARule);
            if (rNegA)
            {
                rNegA->negated = r.negated;
                node->appendBranches(*rNegA);
                // return; // TODO: hack, do not return allow multiple applications of metastatement
            }
        }
        currentMetaNode = currentMetaNode->parentMetastatement;
    }
}

bool Tableau::applyRule(std::shared_ptr<Tableau::Node> node)
{
    if (node->metastatement)
    {
        if (node->metastatement->type == MetastatementType::labelRelation)
        {
            applyRequestRule(this, node);
            // no rule applicable anymore (metastatement)
            return false; // TODO: this value may be true
        }
        else
        {
            applyPropagationRule(this, node);
            return true;
        }
    }
    else if (node->relation)
    {
        if (node->relation->isNormal())
        {
            if (node->relation->negated)
            {
                if (applyRequestRuleNoMeta(this, node))
                {
                    return true;
                }
            }
            else
            {
                auto rA = aRule(*node->relation);
                if (rA)
                {
                    auto &[r1, metastatement] = *rA;
                    r1.negated = node->relation->negated;
                    node->appendBranches(r1);
                    node->appendBranches(*metastatement);
                    return true;
                }
            }
        }
        else if (applyDNFRule(node))
        {
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
        exportProof("infinite");
        applyRule(currentNode);
    }
}

// helper
std::vector<Clause> extractDNF(std::shared_ptr<Tableau::Node> node)
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
        std::vector<Clause> left = extractDNF(node->leftNode);
        std::vector<Clause> right = extractDNF(node->rightNode);
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

std::vector<Clause> Tableau::DNF()
{
    while (!unreducedNodes.empty())
    {
        auto currentNode = unreducedNodes.top();
        unreducedNodes.pop();
        exportProof("dnfcalc");
        if (currentNode->metastatement)
        {
            // only equality meatstatement possible
            applyPropagationRule(this, currentNode);
        }
        else
        {
            applyDNFRule(currentNode);
        }
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
    std::shared_ptr<Node> node = rootNode;
    std::optional<Relation> oldPositive;
    std::optional<Relation> newPositive;
    std::vector<int> activeLabels;
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
            std::vector<int> relationLabels = node->relation->labels();
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

void Tableau::toDotFormat(std::ofstream &output) const
{
    output << "graph {" << std::endl
           << "node[shape=\"plaintext\"]" << std::endl;
    rootNode->toDotFormat(output);
    output << "}" << std::endl;
}

void Tableau::exportProof(std::string filename) const
{
    std::ofstream file(filename + ".dot");
    toDotFormat(file);
}
