#include "Solver.h"
#include <iostream>
#include <fstream>
#include <memory>
#include <algorithm>
#include <iterator>
#include <string>
#include <future>
#include <queue>
#include <stack>
#include "CatInferVisitor.h"
#include "Constraint.h"

using namespace std;

// helper
void dismissSiblings(shared_ptr<ProofNode> node)
{
    if (node->parent != nullptr)
    {
        if (node->parent->leftNode != nullptr && node->parent->leftNode->status == ProofNodeStatus::none)
        {
            node->parent->leftNode->status = ProofNodeStatus::dismiss;
        }
        if (node->parent->rightNode != nullptr && node->parent->rightNode->status == ProofNodeStatus::none)
        {
            node->parent->rightNode->status = ProofNodeStatus::dismiss;
        }
    }
}

vector<shared_ptr<Relation>> getSpan(shared_ptr<Relation> relation, Operator op)
{
    vector<shared_ptr<Relation>> span;
    stack<shared_ptr<Relation>> stack;
    stack.push(relation);

    while (!stack.empty())
    {
        shared_ptr<Relation> tos = stack.top();
        stack.pop();
        if (tos->op == op)
        {
            stack.push(tos->right);
            stack.push(tos->left);
        }
        else
        {
            span.push_back(tos);
        }
    }
    return span;
}

Solver::Solver() {}
Solver::~Solver() {}

map<int, set<shared_ptr<ProofNode>>> Solver::unprovable;
set<shared_ptr<ProofNode>> Solver::proved;
shared_ptr<ProofNode> Solver::root;
shared_ptr<Solver> Solver::rootSolver;
int Solver::iterations = 0;
int Solver::steps = 0;

void Solver::log(string message, int requiredLevel = 2)
{
    if (logLevel >= requiredLevel)
    {
        cout << message << endl;
    }
}

bool Solver::appendProofNodes(ProofRule rule, shared_ptr<ProofNode> leftNode, shared_ptr<ProofNode> rightNode = nullptr)
{
    // check if nodes should be appended
    for (auto node : {leftNode, rightNode})
    {
        if (node == nullptr)
        {
            continue;
        }
        if (node->relationString().length() > 400)
        { // dismiss too complex nodes
            log("Complex node");
            return false;
        }
        // check if cyclic
        shared_ptr<ProofNode> currentNode = node->parent;
        while (currentNode != nullptr)
        {
            if (*currentNode == *node)
            {
                return false;
            }
            currentNode = currentNode->parent;
        }
    }

    // append nodes
    shared_ptr<ProofNode> parentNode = leftNode->parent;
    parentNode->leftNode = leftNode;
    parentNode->rightNode = rightNode;
    parentNode->appliedRule = rule;
    return true;
}
shared_ptr<ProofNode> Solver::newChildProofNode(shared_ptr<ProofNode> node)
{
    shared_ptr<ProofNode> newNode = make_shared<ProofNode>(*node);
    newNode->leftNode = nullptr;
    newNode->rightNode = nullptr;
    newNode->parent = node;
    newNode->appliedRule = ProofRule::none;
    newNode->status = ProofNodeStatus::none;
    newNode->marked = false;
    // left and right relation pointer remain untouched -> update in caller
    return newNode;
}

bool Solver::axiomEmpty(shared_ptr<ProofNode> node)
{
    if (node->left.find(Relation::EMPTY) != node->left.end())
    {
        node->appliedRule = ProofRule::axiomEmpty;
        node->status = ProofNodeStatus::closed;
        goals.pop();
        return true;
    }
    return false;
}
bool Solver::axiomFull(shared_ptr<ProofNode> node)
{
    if (node->right.find(Relation::FULL) != node->right.end())
    {
        node->appliedRule = ProofRule::axiomFull;
        node->status = ProofNodeStatus::closed;
        goals.pop();
        return true;
    }
    return false;
}
bool Solver::axiomEqual(shared_ptr<ProofNode> node)
{
    for (auto r1 = node->left.begin(); r1 != node->left.end(); r1++)
    {
        if (node->right.find(*r1) != node->right.end())
        {
            node->appliedRule = ProofRule::axiomEqual;
            node->status = ProofNodeStatus::closed;
            goals.pop();
            return true;
        }
    }
    return false;
}
bool Solver::andLeftRule(shared_ptr<ProofNode> node)
{
    for (auto relation : node->left)
    {
        if (relation->op == Operator::cap)
        {
            shared_ptr<ProofNode> newNode = newChildProofNode(node);
            newNode->left.erase(relation);
            newNode->left.insert(relation->left);
            newNode->left.insert(relation->right);

            if (appendProofNodes(ProofRule::andLeft, newNode))
            {
                goals.push(newNode);
            }
            else
            {
                node->status = ProofNodeStatus::dismiss;
            }
            return true;
        }
    }
    return false;
}
bool Solver::orRightRule(shared_ptr<ProofNode> node)
{
    for (auto relation : node->right)
    {
        if (relation->op == Operator::cup)
        {
            shared_ptr<ProofNode> newNode = newChildProofNode(node);
            newNode->right.erase(relation);
            newNode->right.insert(relation->left);
            newNode->right.insert(relation->right);

            if (appendProofNodes(ProofRule::orRight, newNode))
            {
                goals.push(newNode);
            }
            else
            {
                node->status = ProofNodeStatus::dismiss;
            }
            return true;
        }
    }
    return false;
}
bool Solver::andRightRule(shared_ptr<ProofNode> node)
{
    for (auto relation : node->right)
    {
        if (relation->op == Operator::cap)
        {
            shared_ptr<ProofNode> newNode1 = newChildProofNode(node);
            shared_ptr<ProofNode> newNode2 = newChildProofNode(node);
            newNode1->right.erase(relation);
            newNode2->right.erase(relation);
            newNode1->right.insert(relation->left);
            newNode2->right.insert(relation->right);

            if (appendProofNodes(ProofRule::andRight, newNode1, newNode2))
            {
                goals.push(newNode1);
                goals.push(newNode2);
            }
            else
            {
                node->status = ProofNodeStatus::dismiss;
            }
            return true;
        }
    }
    return false;
}
bool Solver::orLeftRule(shared_ptr<ProofNode> node)
{
    for (auto relation : node->left)
    {
        if (relation->op == Operator::cup)
        {
            shared_ptr<ProofNode> newNode1 = newChildProofNode(node);
            shared_ptr<ProofNode> newNode2 = newChildProofNode(node);
            newNode1->left.erase(relation);
            newNode2->left.erase(relation);
            newNode1->left.insert(relation->left);
            newNode2->left.insert(relation->right);

            if (appendProofNodes(ProofRule::orLeft, newNode1, newNode2))
            {
                goals.push(newNode1);
                goals.push(newNode2);
            }
            else
            {
                node->status = ProofNodeStatus::dismiss;
            }
            return true;
        }
    }
    return false;
}

bool Solver::inverseRule(shared_ptr<ProofNode> node)
{
    // TODO split in single rules
    for (auto r1 : node->left)
    {
        if (r1->op == Operator::inverse)
        {
            if (r1->left->op == Operator::inverse)
            {
                shared_ptr<ProofNode> newNode = newChildProofNode(node);
                newNode->left.erase(r1);
                newNode->left.insert(r1->left->left);

                if (appendProofNodes(ProofRule::inverse, newNode))
                {
                    goals.push(newNode);
                    return true;
                }
            }
            else
            {
                for (auto r2 : node->right)
                {
                    if (r2->op == Operator::inverse)
                    {
                        if (r2->left->op == Operator::inverse)
                        {
                            shared_ptr<ProofNode> newNode = newChildProofNode(node);
                            newNode->right.erase(r2);
                            newNode->right.insert(r2->left->left);

                            if (appendProofNodes(ProofRule::inverse, newNode))
                            {
                                goals.push(newNode);
                                return true;
                            }
                        }
                        else
                        {
                            // TODO try inverse (need to remove rest knowledge)
                        }
                    }
                }
            }
        }
    }
    return false;
}

bool Solver::distributiveCupRule(shared_ptr<ProofNode> node)
{
    for (auto r1 : node->left)
    {
        if (r1->op == Operator::composition)
        {
            vector<shared_ptr<Relation>> span = getSpan(r1, Operator::composition);
            for (auto r = span.begin(); r != span.end(); r++)
            {
                if ((*r)->op == Operator::cup)
                {
                    int index = r - span.begin();
                    shared_ptr<Relation> left = (*r)->left;
                    shared_ptr<Relation> right = (*r)->right;

                    shared_ptr<ProofNode> newNode1 = newChildProofNode(node);
                    shared_ptr<Relation> newR1;
                    span[index] = left;
                    for (auto r = span.begin(); r != span.end(); r++)
                    {
                        if (newR1 == nullptr)
                        {
                            newR1 = *r;
                        }
                        else
                        {
                            newR1 = make_shared<Relation>(Operator::composition, newR1, *r);
                        }
                    }
                    newNode1->left.erase(r1);
                    newNode1->left.insert(newR1);

                    shared_ptr<ProofNode> newNode2 = newChildProofNode(node);
                    newR1 = nullptr;
                    span[index] = right;
                    for (auto r = span.begin(); r != span.end(); r++)
                    {
                        if (newR1 == nullptr)
                        {
                            newR1 = *r;
                        }
                        else
                        {
                            newR1 = make_shared<Relation>(Operator::composition, newR1, *r);
                        }
                    }
                    newNode2->left.erase(r1);
                    newNode2->left.insert(newR1);

                    if (appendProofNodes(ProofRule::distributiveCup, newNode1, newNode2))
                    {
                        goals.push(newNode1);
                        goals.push(newNode2);
                        return true;
                    }
                }
            }
        }
    }
    return false;
}
bool Solver::seqLeftRule(shared_ptr<ProofNode> node)
{
    // remove id
    for (auto r1 : node->left)
    {
        if (r1->op == Operator::composition)
        {
            vector<shared_ptr<Relation>> span = getSpan(r1, Operator::composition);
            for (auto r = span.begin(); r != span.end(); r++)
            {
                if ((*r) == Relation::ID) // TODO compare pointers here on base relations is valid
                {
                    shared_ptr<ProofNode> newNode1 = newChildProofNode(node);
                    shared_ptr<Relation> newR1;
                    for (auto r = span.begin(); r != span.end(); r++)
                    {
                        if ((*r) != Relation::ID)
                        {
                            if (newR1 == nullptr)
                            {
                                newR1 = *r;
                            }
                            else
                            {
                                newR1 = make_shared<Relation>(Operator::composition, newR1, *r);
                            }
                        }
                    }
                    newNode1->left.erase(r1);
                    newNode1->left.insert(newR1);

                    if (appendProofNodes(ProofRule::seqLeft, newNode1))
                    {
                        goals.push(newNode1);
                        return true;
                    }
                }
            }
        }
    }

    // splits
    for (auto r1 : node->left)
    {
        if (r1->op == Operator::composition)
        {
            // try different splits
            for (auto r2 : node->right)
            {
                if (r2->op != Operator::composition) // TODO make splits over arbitrary
                {
                    continue;
                }
                shared_ptr<ProofNode> newNode1 = newChildProofNode(node);
                shared_ptr<ProofNode> newNode2 = newChildProofNode(node);
                newNode1->left = {r1->left};
                newNode1->right = {r2->left};
                newNode2->left = {r1->right};
                newNode2->right = {r2->right};

                if (appendProofNodes(ProofRule::seqLeft, newNode1, newNode2) && solve({newNode1, newNode2}))
                {
                    return true;
                }
            }
        }
    }

    return false;
}

// TODO count as cons application? first transitive then simplify?
bool Solver::simplifyTcRule(shared_ptr<ProofNode> node) // current:  aa=0, (a|b)+ -> (a|id)(bab|b)+(a|id)
{
    log("[Rule] Simplify TC");
    // TODO: rename unroll left
    // int unrollBound = 2;
    for (auto r : node->left)
    {
        if (r->op == Operator::transitive)
        {
            // TODO: use span function
            RelationSet underlying;
            queue<shared_ptr<Relation>> current;
            current.push(r->left);
            while (!current.empty())
            {
                if (current.front()->op == Operator::cup)
                {
                    current.push(current.front()->left);
                    current.push(current.front()->right);
                    current.pop();
                }
                else
                {
                    underlying.insert(current.front());
                    current.pop();
                }
            }

            for (auto r1 : underlying)
            {
                // TODO: for (auto r2 : node->left)
                //{
                shared_ptr<ProofNode> newNode = newChildProofNode(node);
                shared_ptr<Relation> comp = make_shared<Relation>(Operator::composition, r1, r1);
                newNode->left = {comp};
                newNode->right.clear();

                if (appendProofNodes(ProofRule::simplifyTc, newNode) && solve({newNode}))
                {
                    // We know: r1;r1 <= 0
                    /* TODO: old syymetric: underlying.erase(r1);
                    shared_ptr<ProofNode> newNode2 = newChildProofNode(node);
                    node->rightNode = newNode2;

                    shared_ptr<Relation> unionR = Relation::get("0");
                    for (auto r0 : underlying)
                    {
                        unionR = make_shared<Relation>(Operator::cup, unionR, r0);
                        for (auto r01 : underlying)
                        {
                            shared_ptr<Relation> comp = make_shared<Relation>(Operator::composition, r0, r1);
                            shared_ptr<Relation> comp2 = make_shared<Relation>(Operator::composition, comp, r01);
                            unionR = make_shared<Relation>(Operator::cup, unionR, comp2);
                        }
                    }
                    shared_ptr<Relation> newTc = make_shared<Relation>(Operator::transitive, unionR);
                    shared_ptr<Relation> aOrId = make_shared<Relation>(Operator::cup, r1, Relation::get("id"));
                    shared_ptr<Relation> comp = make_shared<Relation>(Operator::composition, aOrId, newTc);
                    shared_ptr<Relation> comp2 = make_shared<Relation>(Operator::composition, comp, aOrId);

                    newNode2->left.erase(r);
                    newNode2->left.insert(make_shared<Relation>(Operator::cup, comp2, r1));
                    goals.push(newNode2);*/
                    underlying.erase(r1);
                    shared_ptr<ProofNode> newNode2 = newChildProofNode(node);
                    node->rightNode = newNode2;

                    shared_ptr<Relation> unionR;
                    for (auto r0 : underlying)
                    {
                        if (unionR == nullptr)
                        {
                            unionR = r0;
                        }
                        else
                        {
                            unionR = make_shared<Relation>(Operator::cup, unionR, r0);
                        }
                        shared_ptr<Relation> comp = make_shared<Relation>(Operator::composition, r0, r1);
                        unionR = make_shared<Relation>(Operator::cup, unionR, comp);
                    }
                    shared_ptr<Relation> newTc = make_shared<Relation>(Operator::transitive, unionR);
                    shared_ptr<Relation> aOrId = make_shared<Relation>(Operator::cup, r1, Relation::ID);
                    shared_ptr<Relation> comp = make_shared<Relation>(Operator::composition, aOrId, newTc);

                    newNode2->left.erase(r);
                    newNode2->left.insert(make_shared<Relation>(Operator::cup, comp, r1));
                    goals.push(newNode2);
                    return true;
                }
                //}
            }
        }
    }
    /*for (auto r : node->left)
    {
        if (r->op == Operator::transitive)
        {
            shared_ptr<ProofNode> newNode1 = newChildProofNode(node);
            shared_ptr<ProofNode> newNode2 = newChildProofNode(node);
            newNode1->left.erase(r);
            newNode2->left.erase(r);

            shared_ptr<Relation> unrolledR = r->left;
            shared_ptr<Relation> unrolledR2 = make_shared<Relation>(Operator::composition, unrolledR, unrolledR);
            shared_ptr<Relation> unrolledR2Tc = make_shared<Relation>(Operator::transitive, unrolledR2);
            shared_ptr<Relation> unrolledRCompTc = make_shared<Relation>(Operator::composition, unrolledR, unrolledR2Tc);
            newNode1->left.insert(unrolledRCompTc);
            newNode2->left.insert(unrolledR2Tc);

            if (!isCycle(newNode1) && !isCycle(newNode2))
            {
                node->leftNode = newNode1;
                node->rightNode = newNode2;
                node->appliedRule = ProofRule::simplifyTc;
                goals.push(newNode1);
                goals.push(newNode2);
                return true;
            }
        }
    }*/
    return false;
}

bool Solver::transitiveClosureRule(shared_ptr<ProofNode> node)
{
    for (auto r1 : node->left)
    {
        if (r1->op == Operator::transitive)
        {
            for (auto r2 : node->right)
            {
                if (r2->op == Operator::transitive)
                {
                    // simple case
                    shared_ptr<ProofNode> newNode1 = newChildProofNode(node);
                    newNode1->left = {r1->left};
                    newNode1->right = {r2};

                    if (appendProofNodes(ProofRule::transitiveClosure, newNode1) && solve({newNode1}))
                    {
                        return true;
                    }
                }
                else
                {
                    shared_ptr<ProofNode> newNode1 = newChildProofNode(node);
                    shared_ptr<ProofNode> newNode2 = newChildProofNode(node);
                    newNode1->left = {r1->left};
                    newNode1->right = {r2};
                    newNode2->left = {make_shared<Relation>(Operator::composition, r2, r2)};
                    newNode2->right = {r2};

                    if (appendProofNodes(ProofRule::transitiveClosure, newNode1, newNode2) && solve({newNode1, newNode2}))
                    {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

bool Solver::unrollRule(shared_ptr<ProofNode> node)
{
    for (auto r : node->right)
    {
        if (r->op == Operator::transitive)
        {
            shared_ptr<ProofNode> newNode = newChildProofNode(node);
            // newNode->right.erase(r); // TODO: better dont erease remember for later unrolls
            shared_ptr<Relation> unrolledR = r->left;
            /* TODO old: for (auto i = 0; i < unrollBound; i++)
            {
                newNode->right.insert(unrolledR);
                unrolledR = make_shared<Relation>(Operator::composition, unrolledR, r->left);
            }*/

            // TODO: new
            newNode->right.erase(r);
            newNode->right.insert(unrolledR);
            newNode->right.insert(make_shared<Relation>(Operator::composition, r, r));

            if (appendProofNodes(ProofRule::unroll, newNode))
            {
                goals.push(newNode);
                return true;
            }
        }
    }
    return false;
}

double heuristicVal(shared_ptr<ProofNode> node, Inequality inequality)
{
    bool satisfiedLeft = true;
    inequality->left.erase(Relation::FULL); // TODO: where handle this? empty side equals full
    for (auto r2 : inequality->left)
    {
        bool found = false;
        for (auto r1 : node->left)
        {
            if (*r1 == *r2)
            {
                found = true;
            }
        }
        if (!found)
        {
            satisfiedLeft = false;
        }
    }
    if (inequality->left.size() == 1 && *inequality->left.begin()->get() == *Relation::FULL)
    {
        // TODO same for the emmpty side
        satisfiedLeft = true; // TODO: where handle this? empty side equals full
    }
    bool satisfiedRight = true;
    inequality->right.erase(Relation::EMPTY); // TODO: where handle this? empty side equals empty
    for (auto r2 : inequality->right)
    {
        // TODO right side needs to be emptycurrently
        bool found = false;
        for (auto r1 : node->right)
        {
            if (*r1 == *r2)
            {
                found = true;
            }
        }
        if (!found)
        {
            satisfiedRight = false;
        }
    }
    if (satisfiedLeft && satisfiedRight)
    {
        return 2 + (1 / inequality->relationString().length()); // TODO complexity of term
    }
    else if (satisfiedLeft || satisfiedRight)
    {
        return 1 + (1 / inequality->relationString().length());
    }
    return 0;
}

vector<Inequality> consHeuristic(shared_ptr<ProofNode> node, Theory &theory)
{
    vector<Inequality> arr(theory.begin(), theory.end());
    sort(arr.begin(), arr.end(), [node](Inequality a, Inequality b)
         { return heuristicVal(node, a) > heuristicVal(node, b); });
    return arr;
}

bool Solver::consRule(shared_ptr<ProofNode> node)
{
    if (node->currentConsDepth <= 0)
    {
        return false;
    }
    vector<Inequality> sorted = consHeuristic(node, theory);

    // theory applicationsto whole relation
    for (auto iequ = sorted.begin(); iequ != sorted.end(); iequ++)
    {
        Inequality inequality = *iequ;
        if (heuristicVal(node, inequality) > 0)
        {
            shared_ptr<ProofNode> newNode1 = newChildProofNode(node);
            shared_ptr<ProofNode> newNode2 = newChildProofNode(node);
            if (inequality->right.empty())
            {
                newNode1->left.insert(Relation::EMPTY);
            }
            if (inequality->left.empty())
            {
                newNode2->right.insert(Relation::FULL);
            }
            for (auto n : inequality->right)
            {
                newNode1->left.insert(n);
            }
            for (auto n : inequality->left)
            {
                newNode2->right.insert(n);
            }

            newNode1->currentConsDepth--;
            newNode2->currentConsDepth--;

            if (appendProofNodes(ProofRule::cons, newNode1, newNode2) && solve({newNode1, newNode2}))
            {
                return true;
            }
        }
    }
    // application to subrelations
    for (auto r : node->left)
    {
        if (r->op == Operator::composition)
        {
            for (auto iequ = sorted.begin(); iequ != sorted.end(); iequ++)
            {
                Inequality inequality = *iequ;
                // TODO: in the moment only splitting inequ >= int | ext (but this is also possible a<=b|c with 1!<=b|c)
                inequality->left.erase(Relation::FULL);
                auto rightInequ = inequality->right.begin();
                if (inequality->left.empty() && (*rightInequ)->op == Operator::cup)
                {
                    shared_ptr<ProofNode> newNode1 = newChildProofNode(node);
                    shared_ptr<ProofNode> newNode12 = newChildProofNode(node);
                    shared_ptr<ProofNode> newNode2 = newChildProofNode(node);
                    shared_ptr<ProofNode> newNode22 = newChildProofNode(node);
                    shared_ptr<Relation> newR1 = make_shared<Relation>(*r);
                    shared_ptr<Relation> newR2 = make_shared<Relation>(*r);
                    shared_ptr<Relation> rleftCapInequLeft = make_shared<Relation>(Operator::cap, r->left, (*rightInequ)->left);
                    shared_ptr<Relation> rleftCapInequRight = make_shared<Relation>(Operator::cap, r->left, (*rightInequ)->right);
                    shared_ptr<Relation> rrightCapInequLeft = make_shared<Relation>(Operator::cap, r->right, (*rightInequ)->left);
                    shared_ptr<Relation> rrightCapInequRight = make_shared<Relation>(Operator::cap, r->right, (*rightInequ)->right);
                    newR1->left = make_shared<Relation>(Operator::cup, rleftCapInequLeft, rleftCapInequRight);
                    newR2->right = make_shared<Relation>(Operator::cup, rrightCapInequLeft, rrightCapInequRight);
                    newNode1->left.erase(r);
                    newNode2->left.erase(r);
                    newNode1->left.insert(newR1);
                    newNode2->left.insert(newR2);
                    newNode22->right.clear();

                    for (auto r0 = newNode2->left.begin(); r0 != newNode2->left.end(); r0++)
                    {
                        newNode22->right.insert(*r0);
                    }
                    // newNode12->right = RelationSet(newNode1->left);
                    // newNode22->right = RelationSet(newNode2->left);

                    newNode1->currentConsDepth--;
                    newNode2->currentConsDepth--;
                    newNode12->currentConsDepth--;
                    newNode22->currentConsDepth--;
                    if (appendProofNodes(ProofRule::cons, newNode1) && solve({newNode1})) // TODO: newNode12
                    {
                        return true;
                    }
                    else if (appendProofNodes(ProofRule::cons, newNode2) && solve({newNode2})) // TODO: newNode22
                    {
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

bool Solver::weakRightRule(shared_ptr<ProofNode> node)
{
    for (auto r : node->right)
    {
        shared_ptr<ProofNode> newNode = newChildProofNode(node);
        newNode->right.erase(r);

        if (appendProofNodes(ProofRule::weakRight, newNode) && solve({newNode}))
        {
            return true;
        }
    }
    return false;
}
bool Solver::weakLeftRule(shared_ptr<ProofNode> node)
{
    for (auto r : node->left)
    {
        shared_ptr<ProofNode> newNode = newChildProofNode(node);
        newNode->left.erase(r);
        if (appendProofNodes(ProofRule::weakLeft, newNode) && solve({newNode}))
        {
            return true;
        }
    }
    return false;
}

bool Solver::loopRule(shared_ptr<ProofNode> node)
{
    for (auto r1 : node->left)
    {
        for (auto r2 : node->right)
        {
            if (*r1 == *Relation::ID && r2->op == Operator::composition)
            {
                shared_ptr<ProofNode> newNode1 = newChildProofNode(node);
                shared_ptr<ProofNode> newNode2 = newChildProofNode(node);
                newNode1->right = {r2->left};
                newNode2->right = {r2->right};

                if (appendProofNodes(ProofRule::loop, newNode1, newNode2) && solve({newNode1, newNode2}))
                {
                    return true;
                }
            }
        }
    }
    return false;
}

bool Solver::invcapEmptyRule(shared_ptr<ProofNode> node)
{
    shared_ptr<Relation> r0 = nullptr; // inverses
    shared_ptr<Relation> r1 = nullptr; // normal
    for (auto r : node->left)
    {
        if (r->op == Operator::inverse)
        {
            if (r0 == nullptr)
            {
                r0 = r->left;
            }
            else
            {
                r0 = make_shared<Relation>(Operator::cap, r0, r->left);
            }
        }
        else
        {
            if (r1 == nullptr)
            {
                r1 = r;
            }
            else
            {
                r1 = make_shared<Relation>(Operator::cap, r1, r);
            }
        }
    }

    if (r0 != nullptr && r1 != nullptr)
    {
        shared_ptr<ProofNode> newNode = newChildProofNode(node);
        shared_ptr<Relation> rComp = make_shared<Relation>(Operator::composition, r0, r1);
        newNode->left = {rComp, Relation::ID};
        newNode->right.clear();

        if (appendProofNodes(ProofRule::empty, newNode))
        {
            goals.push(newNode);
            return true;
        }
    }
    return false;
}

bool Solver::idseqEmptyRule(shared_ptr<ProofNode> node)
{
    for (auto r1 : node->left)
    {
        if (*r1 == *Relation::ID)
        {
            for (auto r2 : node->left)
            {
                if (r2->op == Operator::composition)
                {
                    shared_ptr<ProofNode> newNode = newChildProofNode(node);
                    shared_ptr<Relation> linv = make_shared<Relation>(Operator::inverse, r2->left);
                    newNode->left = {linv, r2->right};
                    newNode->right.clear();

                    if (appendProofNodes(ProofRule::empty, newNode))
                    {
                        goals.push(newNode);
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

void Solver::load(string model1, string model2)
{
    shared_ptr<ProofNode> goal = make_shared<ProofNode>();
    CatInferVisitor visitor;

    // first program
    ConstraintSet sc = visitor.parse(model1);
    // new load: load as inequality in theory
    // TODO theory application: simple adding left and right and split
    for (auto &[name, constraint] : sc)
    {
        constraint.toEmptyNormalForm();

        shared_ptr<ProofNode> inequality = make_shared<ProofNode>();
        inequality->left.insert(constraint.relation);
        // inequality->right.insert(Relation::get("0")); // implicit empty relation on right side
        theory.insert(inequality);

        goal->right.insert(constraint.relation); // instant apply of constraint theorems
    }

    // second program
    ConstraintSet tso = visitor.parse(model2);
    shared_ptr<Relation> r = nullptr;
    for (auto &[name, constraint] : tso)
    {
        constraint.toEmptyNormalForm();
        if (r == nullptr)
        {
            r = constraint.relation;
            continue;
        }
        r = make_shared<Relation>(Operator::cup, r, constraint.relation);
    }
    goal->left.insert(r);

    goals.push(goal);
}

void Solver::learnGoal(shared_ptr<ProofNode> node)
{
    shared_ptr<ProofNode> provenRule = make_shared<ProofNode>(*node);
    provenRule->parent = nullptr;
    Solver::proved.insert(provenRule);
}

void Solver::closeCurrentGoal()
{
    shared_ptr<ProofNode> currentGoal = goals.top();
    currentGoal->status = ProofNodeStatus::closed;
    learnGoal(currentGoal);
    goals.pop();
}

void Solver::dismissCurrentGoal()
{
    shared_ptr<ProofNode> currentGoal = goals.top();
    currentGoal->status = ProofNodeStatus::dismiss;
    dismissSiblings(currentGoal);
    goals.pop();
}

bool Solver::solve()
{
    shared_ptr<ProofNode> root = goals.top(); // needed check if rootSolver
    if (Solver::root == nullptr)
    {
        Solver::root = root;
    }
    if (Solver::rootSolver == nullptr)
    {
        log("Start solving");
        Solver::rootSolver = shared_ptr<Solver>(this);
    }
    while (!goals.empty())
    {
        Solver::iterations++;
        shared_ptr<ProofNode> currentGoal = goals.top();
        log("|= " + currentGoal->relationString());
        if (steps > -1 || currentGoal->marked)
        {
            if (steps > 1)
            {
                steps--;
            }
            else
            {
                exportProof("proof", currentGoal);
                string input;
                getline(cin, input);
                int newSteps = stoi(input);
                if (currentGoal->marked)
                {
                    currentGoal->marked = false;
                    steps = 0;
                }
                else if (newSteps > -2)
                {
                    steps = newSteps;
                }
                else if (steps == -2)
                {
                    currentGoal->marked = true;
                    steps = -1;
                }
            }
        }

        assert(currentGoal->status != ProofNodeStatus::closed);
        if (currentGoal->status == ProofNodeStatus::dismiss)
        {
            log("Current goal has been marked to dismiss.");
            goals.pop();
            continue;
        }

        bool skipGoal = false;
        // check if currentGoal is relative unprovable
        // TODO performance with hashing
        /* for (auto failed : unprovable[currentGoal->currentConsDepth])
        {
            if (*failed == *currentGoal)
            {
                log("Goal is probably not provable.");
                currentGoal->status = ProofNodeStatus::dismiss;
                dismissSiblings(currentGoal);
                goals.pop();
                skipGoal = true;
            }
        } //*/
        // check if proven
        for (auto good : Solver::proved)
        {
            if (*good == *currentGoal)
            {
                log("Goal is known closed goal.");
                if (currentGoal->parent != nullptr)
                {
                    if (currentGoal->parent->leftNode == currentGoal)
                    {
                        currentGoal->parent->leftNode = good;
                    }
                    else
                    {
                        currentGoal->parent->rightNode = good;
                    }
                }
                currentGoal->status = ProofNodeStatus::closed;
                goals.pop();
                skipGoal = true;
            }
        }
        if (skipGoal)
        {
            continue;
        }

        // nodes children are closed
        bool hasRuleApplied = currentGoal->appliedRule != ProofRule(ProofRule::none);
        bool leftClosed = currentGoal->leftNode == nullptr || currentGoal->leftNode->status == ProofNodeStatus::closed;
        bool rightClosed = currentGoal->rightNode == nullptr || currentGoal->rightNode->status == ProofNodeStatus::closed;
        if (hasRuleApplied)
        {
            if (leftClosed && rightClosed)
            {
                log("Close goal since all childrens are closed.");
                closeCurrentGoal();
                continue;
            }

            // must do rules, do not try other rule // TODO must-do == gdw rules
            if (currentGoal->appliedRule.iffRule())
            {
                unprovable[currentGoal->currentConsDepth].insert(currentGoal); // TODO: antilearn Function
                dismissCurrentGoal();
                continue;
            }
        }

        // apply next rule
        log("Apply next rule.");
        bool done = false;
        switch (currentGoal->appliedRule)
        {
        case ProofRule::none:
            done = axiomEmpty(currentGoal);
        case ProofRule::axiomEmpty:
            done = !done ? axiomFull(currentGoal) : done;
        case ProofRule::axiomFull:
            done = !done ? axiomEqual(currentGoal) : done;
        case ProofRule::axiomEqual:
            done = !done ? andLeftRule(currentGoal) : done;
        case ProofRule::andLeft:
            done = !done ? orRightRule(currentGoal) : done;
        case ProofRule::orRight:
            done = !done ? orLeftRule(currentGoal) : done;
        case ProofRule::orLeft:
            done = !done ? andRightRule(currentGoal) : done;
        case ProofRule::andRight:
            done = !done ? inverseRule(currentGoal) : done;
        case ProofRule::inverse:
            done = !done ? distributiveCupRule(currentGoal) : done;
        case ProofRule::distributiveCup:
            done = !done ? seqLeftRule(currentGoal) : done;
        case ProofRule::seqLeft:
            done = !done ? transitiveClosureRule(currentGoal) : done;
        case ProofRule::transitiveClosure:
            done = !done ? unrollRule(currentGoal) : done;
        case ProofRule::unroll:
            done = !done ? simplifyTcRule(currentGoal) : done;
        case ProofRule::simplifyTc:
            done = !done ? consRule(currentGoal) : done;
        case ProofRule::cons:
            // TODO: reactivate done = !done ? loopRule(currentGoal) : done;
        case ProofRule::loop:
            // done = !done ? weakRightRule(currentGoal) : done;
        case ProofRule::weakRight:
            // done = !done ? weakLeftRule(currentGoal) : done;
        case ProofRule::weakLeft:
            // todo special rules
            done = !done ? invcapEmptyRule(currentGoal) : done;
            // TODO: terative? done = !done ? idseqEmptyRule(currentGoal) : done;
        case ProofRule::empty:
            break;
        default:
            log("Error", 1);
            break;
        }

        if (!done)
        {
            log("No rule is applicable anymore.");
            unprovable[currentGoal->currentConsDepth].insert(currentGoal); // TODO: antilearn Function
            dismissCurrentGoal();
        }
    }

    if (Solver::root == root && root->status != ProofNodeStatus::closed)
    {
        // ierative deepening on root Solver
        goals.push(root);
        root->appliedRule = ProofRule::none;
        root->status = ProofNodeStatus::none;
        root->leftNode = nullptr;
        root->rightNode = nullptr;
        root->currentConsDepth++;
        if (steps == -3)
        {
            steps = 0;
        }
        log("# Increase consRule depth bound to " + to_string(root->currentConsDepth), 1);
        solve();
    }

    return root->status == ProofNodeStatus::closed;
}

// helper functions
void Solver::reset()
{
    // clear goals and push new goal
    while (!goals.empty())
    {
        goals.pop();
    }
    steps = 0;
    Solver::root = nullptr;
}
bool Solver::solve(string model1, string model2)
{
    reset();
    load(model1, model2);
    bool solved = solve();
    cout << model1 << "<=" << model2 << ": " << solved << endl;
    return solved;
}
bool Solver::solve(Inequality goal)
{
    reset();
    goals.push(goal);
    bool solved = solve();
    cout << goal->relationString() << ": " << solved << endl;
    return solved;
}
bool Solver::solve(initializer_list<shared_ptr<ProofNode>> goals)
{
    /* TODO: future<bool> future = async(launch::async, [goals, this]() -> bool
                                {

    future_status status;
    status = future.wait_for(chrono::seconds(30));

    if (status == std::future_status::timeout)
    {
        return false;
    }
    else if (status == std::future_status::ready)
    {
        // Get result from future (if there's a need)
        return future.get();
    }*/
    for (auto goal : goals)
    {
        Solver subSolver;
        subSolver.theory = theory;
        subSolver.logLevel = logLevel;
        subSolver.goals.push(goal);
        if (!subSolver.solve())
        {
            return false;
        }
    }
    return true;
}

string Solver::toDotFormat(shared_ptr<ProofNode> node, shared_ptr<ProofNode> currentGoal)
{
    // TODO: function: used
    string rootStackString;
    if (logLevel > 1)
    {
        stack<shared_ptr<ProofNode>> goalsCopy = stack<shared_ptr<ProofNode>>(rootSolver->goals);
        while (goalsCopy.size() > 1)
        {
            shared_ptr<ProofNode> tos = goalsCopy.top();
            goalsCopy.pop();
            shared_ptr<ProofNode> secondTos = goalsCopy.top();
            rootStackString.append("\"" + ProofNode::getId(*secondTos) + "\" -> \"" + ProofNode::getId(*tos) + "\" [label=\"stack\", color=blue, constraint=false];\n");
        }
    }

    string stackString;
    if (&*rootSolver != this)
    {
        if (logLevel > 1)
        {
            stack<shared_ptr<ProofNode>> goalsCopy = stack<shared_ptr<ProofNode>>(goals);
            while (goalsCopy.size() > 1)
            {
                shared_ptr<ProofNode> tos = goalsCopy.top();
                goalsCopy.pop();
                shared_ptr<ProofNode> secondTos = goalsCopy.top();
                stackString.append("\"" + ProofNode::getId(*secondTos) + "\" -> \"" + ProofNode::getId(*tos) + "\" [label=\"stack\", color=blue, constraint=false];\n");
            }
        }
    }

    string unprovableGoals;
    for (auto goalSet = unprovable.begin(); goalSet != unprovable.end(); goalSet++)
    {
        int consDepth = (*goalSet).first;
        for (auto goal = (*goalSet).second.begin(); goal != (*goalSet).second.end(); goal++)
        {
            unprovableGoals.append("\"" + to_string(consDepth) + ": " + (*goal)->relationString() + "\";\n");
        }
    }

    // hack: concentrate merges double edges when reusing proof nodes, but also merges parent edge
    return "digraph { \nconcentrate=true\nnode [shape=plain];\n\n\"consDepth: " + to_string(node->currentConsDepth) + "\";\n" + node->toDotFormat(currentGoal) + rootStackString + stackString + /*TODO debugging unprovableGoals +*/ "\n}";
}
void Solver::exportProof(string filename, shared_ptr<ProofNode> currentGoal)
{
    // export proof
    log("Export proof.");
    ofstream dotFile;
    dotFile.open("build/" + filename + ".dot");
    dotFile << toDotFormat(root, currentGoal);
    dotFile.close();
}
