#include "Solver.h"
#include <iostream>
#include <fstream>
#include <memory>
#include <algorithm>
#include <iterator>
#include <string>
#include <future>
#include <queue>
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

Solver::Solver() : stepwise(false)
{
}
Solver::~Solver() {}

map<int, set<shared_ptr<ProofNode>>> Solver::unprovable;
set<shared_ptr<ProofNode>> Solver::proved;
shared_ptr<ProofNode> Solver::root;

void Solver::log(string message)
{
    if (!silent)
    {
        cout << message << endl;
    }
}

// TODO: add node function
bool Solver::isCycle(shared_ptr<ProofNode> node)
{
    // TODO hack: misuse this function to abort too long proof obligatins
    if (node->relationString().length() > 200)
    {
        log("COMPLEX NODE.");
        return true;
    }
    // bounded cons depth
    if (node->currentConsDepth < 0)
    {
        log("CONS DEPTH"); // iterative cons deepening
        return true;
    }
    // check if cyclic
    shared_ptr<ProofNode> currentNode = node->parent;
    while (currentNode != nullptr)
    {
        if (*currentNode.get() == *node.get())
        {
            return true;
        }
        currentNode = currentNode->parent;
    }
    return false;
}

shared_ptr<ProofNode> Solver::childProofNode(shared_ptr<ProofNode> node)
{
    shared_ptr<ProofNode> newNode = make_shared<ProofNode>(*node);
    newNode->leftNode = nullptr;
    newNode->rightNode = nullptr;
    newNode->parent = node;
    newNode->appliedRule = ProofRule::none;
    newNode->status = ProofNodeStatus::none;
    // left and right stay the same child pointer must be set later
    return newNode;
}

bool Solver::axiomEmpty(shared_ptr<ProofNode> node)
{
    /*TODO: also in other functions
    if (node->left.find(Relation::get("0")) != node->left.end())
    {
        node->appliedRule = ProofRule::axiomEmpty;
        node->status = ProofNodeStatus::closed;
        return true;
    }*/
    for (auto r1 : node->left)
    {
        if (*r1 == *Relation::EMPTY)
        {
            node->appliedRule = ProofRule::axiomEmpty;
            node->status = ProofNodeStatus::closed;
            return true;
        }
    }
    return false;
}

bool Solver::axiomFull(shared_ptr<ProofNode> node)
{
    for (auto r1 : node->right)
    {
        if (*r1 == *Relation::FULL)
        {
            node->appliedRule = ProofRule::axiomFull;
            node->status = ProofNodeStatus::closed;
            return true;
        }
    }
    return false;
}

bool Solver::axiomEqual(shared_ptr<ProofNode> node)
{
    // TODO: more efficient way to apply axiom1?
    // RelationSet intersection = {};
    // set_intersection(
    //     node->left.begin(),
    //     node->left.end(),
    //     node->right.begin(),
    //     node->right.end(),
    //     inserter(intersection, intersection.begin()),
    //     [](shared_ptr<Relation> r1, shared_ptr<Relation> r2)
    //     {
    //         return r1->op == r2->op ? lexicographical_compare(r1->alias,, r2->alias) : r1->op < r2->op;
    //     });
    // if (!intersection.empty()) {
    //     node->appliedRule = "axiomEqual";
    //     node->closed = true;
    //     return true;
    // }
    for (auto r1 : node->left)
    {
        for (auto r2 : node->right)
        {
            if (*r1 == *r2)
            {
                node->appliedRule = ProofRule::axiomEqual;
                node->status = ProofNodeStatus::closed;
                return true;
            }
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
            shared_ptr<ProofNode> newNode = childProofNode(node);
            newNode->left.erase(relation);
            newNode->left.insert(relation->left);
            newNode->left.insert(relation->right);

            if (!isCycle(newNode))
            {
                node->leftNode = newNode;
                node->rightNode = nullptr;
                node->appliedRule = ProofRule::andLeft;
                goals.push(newNode);
                return true;
            }
            else
            { // TODO: must rules buggy
                // must rule: if this leads to cycle stop
                node->appliedRule = ProofRule::empty;
                goals.pop();
                return true;
            }
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
            shared_ptr<ProofNode> newNode1 = childProofNode(node);
            shared_ptr<ProofNode> newNode2 = childProofNode(node);
            newNode1->right.erase(relation);
            newNode2->right.erase(relation);
            newNode1->right.insert(relation->left);
            newNode2->right.insert(relation->right);

            if (!isCycle(newNode1) && !isCycle(newNode2))
            {
                node->leftNode = newNode1;
                node->rightNode = newNode2;
                node->appliedRule = ProofRule::andRight;
                goals.push(newNode1);
                goals.push(newNode2);
                return true;
            }
            else
            { // TODO: must rules buggy
              // must rule: if this leads to cycle stop
                goals.pop();
                return true;
            }
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
            shared_ptr<ProofNode> newNode = childProofNode(node);
            newNode->right.erase(relation);
            newNode->right.insert(relation->left);
            newNode->right.insert(relation->right);

            if (!isCycle(newNode))
            {
                node->leftNode = newNode;
                node->rightNode = nullptr;
                node->appliedRule = ProofRule::orRight;
                goals.push(newNode);
                return true;
            }
            else
            { // TODO: must rules buggy
              // must rule: if this leads to cycle stop
                goals.pop();
                return true;
            }
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
            shared_ptr<ProofNode> newNode1 = childProofNode(node);
            shared_ptr<ProofNode> newNode2 = childProofNode(node);
            newNode1->left.erase(relation);
            newNode2->left.erase(relation);
            newNode1->left.insert(relation->left);
            newNode2->left.insert(relation->right);

            if (!isCycle(newNode1) && !isCycle(newNode2)) // TODO make function
            {
                node->leftNode = newNode1;
                node->rightNode = newNode2;
                node->appliedRule = ProofRule::orLeft;
                goals.push(newNode1);
                goals.push(newNode2);
                return true;
            }
            else
            { // TODO: must rules buggy
              // must rule: if this leads to cycle stop
                goals.pop();
                return true;
            }
        }
    }
    return false;
}

bool Solver::inverseRule(shared_ptr<ProofNode> node)
{
    for (auto r1 : node->left)
    {
        if (r1->op == Operator::inverse)
        {
            if (r1->left->op == Operator::inverse)
            {
                shared_ptr<ProofNode> newNode = childProofNode(node);
                newNode->left.erase(r1);
                newNode->left.insert(r1->left->left);

                if (!isCycle(newNode))
                {
                    node->leftNode = newNode;
                    node->rightNode = nullptr;
                    node->appliedRule = ProofRule::inverse;
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
                            shared_ptr<ProofNode> newNode = childProofNode(node);
                            newNode->right.erase(r2);
                            newNode->right.insert(r2->left->left);

                            if (!isCycle(newNode))
                            {
                                node->leftNode = newNode;
                                node->rightNode = nullptr;
                                node->appliedRule = ProofRule::inverse;
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

bool Solver::seqLeftRule(shared_ptr<ProofNode> node)
{
    // distributive
    for (auto r1 : node->left)
    {
        if (r1->op == Operator::composition)
        {
            /* TODO:
            vector<shared_ptr<Relation>> span = getSpan(r1, Operator::composition);
            for (auto r = span.begin(); r != span.end(); r++;)
            {
                shared_ptr<Relation> curr = r->get();
                if (curr->left->op == Operator::cup)
                {
                    shared_ptr<ProofNode> newNode = childProofNode(node);
                    shared_ptr<Relation> r1Copy1 = make_shared<Relation>(*r1);
                    shared_ptr<Relation> r1Copy2 = make_shared<Relation>(*r1);
                    r1Copy1->left = r1->left->left;
                    r1Copy2->left = r1->left->right;
                    newNode->left.erase(r1);
                    newNode->left.insert(r1Copy1);
                    newNode->left.insert(r1Copy2);

                    if (!isCycle(newNode))
                    {
                        node->leftNode = newNode;
                        node->rightNode = nullptr;
                        // TODO: node->appliedRule = ProofRule::seqLeft;
                        goals.push(newNode);
                        return true;
                    }
                }
            }*/
            if (r1->left->op == Operator::cup)
            {
                shared_ptr<ProofNode> newNode = childProofNode(node);
                shared_ptr<Relation> r1Copy1 = make_shared<Relation>(*r1);
                shared_ptr<Relation> r1Copy2 = make_shared<Relation>(*r1);
                r1Copy1->left = r1->left->left;
                r1Copy2->left = r1->left->right;
                newNode->left.erase(r1);
                newNode->left.insert(r1Copy1);
                newNode->left.insert(r1Copy2);

                if (!isCycle(newNode))
                {
                    node->leftNode = newNode;
                    node->rightNode = nullptr;
                    // TODO: node->appliedRule = ProofRule::seqLeft;
                    goals.push(newNode);
                    return true;
                }
            }
            if (r1->right->op == Operator::cup)
            {
                shared_ptr<ProofNode> newNode = childProofNode(node);
                shared_ptr<Relation> r1Copy1 = make_shared<Relation>(*r1);
                shared_ptr<Relation> r1Copy2 = make_shared<Relation>(*r1);
                r1Copy1->right = r1->right->left;
                r1Copy2->right = r1->right->right;
                newNode->left.erase(r1);
                newNode->left.insert(r1Copy1);
                newNode->left.insert(r1Copy2);

                if (!isCycle(newNode))
                {
                    node->leftNode = newNode;
                    node->rightNode = nullptr;
                    // TODO: node->appliedRule = ProofRule::seqLeft;
                    goals.push(newNode);
                    return true;
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
                shared_ptr<ProofNode> newNode1 = childProofNode(node);
                shared_ptr<ProofNode> newNode2 = childProofNode(node);
                newNode1->left = {r1->left};
                newNode1->right = {r2->left};
                newNode2->left = {r1->right};
                newNode2->right = {r2->right};

                if (!isCycle(newNode1) && !isCycle(newNode2))
                {
                    node->leftNode = newNode1;
                    node->rightNode = newNode2;
                    node->appliedRule = ProofRule::seqLeft;

                    if (solve({newNode1, newNode2}))
                    {
                        return true;
                    }
                }
            }
        }
    }
    /*/ TODO: append id
    for (auto r1 : node->left)
    {
        shared_ptr<ProofNode> newNode = childProofNode(node);
        newNode->left.clear();
        shared_ptr<Relation> nr = make_shared<Relation>(Operator::composition, r1, Relation::get("id"));
        newNode->left.insert(nr);

        seqSolver.goals.push(newNode);
        if (seqSolver.solve())
        {
            // successful split
            node->leftNode = newNode;
            node->rightNode = nullptr;
            node->appliedRule = ProofRule::seqLeft; // TODO
            return true;
        }
    }*/
    return false;
}

/// current:  aa=0, (a|b)+ -> (a|id)(bab|b)+(a|id)
bool Solver::simplifyTcRule(shared_ptr<ProofNode> node)
{
    // TODO: rename unroll left
    // int unrollBound = 2;
    for (auto r : node->left)
    {
        if (r->op == Operator::transitive)
        {
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
                shared_ptr<ProofNode> newNode = childProofNode(node);
                shared_ptr<Relation> comp = make_shared<Relation>(Operator::composition, r1, r1);
                newNode->left = {comp};
                newNode->right.clear();

                node->leftNode = newNode;
                node->rightNode = nullptr;
                node->appliedRule = ProofRule::simplifyTc;

                if (solve({newNode}))
                {
                    underlying.erase(r1);
                    shared_ptr<ProofNode> newNode2 = childProofNode(node);
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
            shared_ptr<ProofNode> newNode1 = childProofNode(node);
            shared_ptr<ProofNode> newNode2 = childProofNode(node);
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
                    shared_ptr<ProofNode> newNode1 = childProofNode(node);
                    newNode1->left = {r1->left};
                    newNode1->right = {r2};
                    node->leftNode = newNode1;
                    node->rightNode = nullptr;
                    node->appliedRule = ProofRule::transitiveClosure;

                    if (solve({newNode1}))
                    {
                        return true;
                    }
                }
                else
                {
                    shared_ptr<ProofNode> newNode1 = childProofNode(node);
                    shared_ptr<ProofNode> newNode2 = childProofNode(node);
                    newNode1->left = {r1->left};
                    newNode1->right = {r2};
                    newNode2->left = {make_shared<Relation>(Operator::composition, r2, r2)};
                    newNode2->right = {r2};
                    node->leftNode = newNode1;
                    node->rightNode = newNode2;
                    node->appliedRule = ProofRule::transitiveClosure;

                    if (solve({newNode1, newNode2}))
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
    int unrollBound = 2;
    for (auto r : node->right)
    {
        if (r->op == Operator::transitive)
        {
            shared_ptr<ProofNode> newNode = childProofNode(node);
            newNode->right.erase(r);
            shared_ptr<Relation> unrolledR = r->left;
            for (auto i = 0; i < unrollBound; i++)
            {
                newNode->right.insert(unrolledR);
                unrolledR = make_shared<Relation>(Operator::composition, unrolledR, r->left);
            }

            if (!isCycle(newNode))
            {
                node->leftNode = newNode;
                node->rightNode = nullptr;
                node->appliedRule = ProofRule::unroll;
                goals.push(newNode);
                return true;
            }
        }
    }
    return false;
}

bool Solver::cutRule(shared_ptr<ProofNode> node)
{
    for (auto const &[_, r] : Relation::relations)
    {
        bool isIn = false; // TODO refactor: improve performance
        for (auto l : node->left)
        {
            if (*r == *l)
            {
                isIn = true;
            }
        }
        for (auto l : node->right)
        {
            if (*r == *l)
            {
                isIn = true;
            }
        }
        // TODO: save elemts in sets not pointers to allow membership checks like: && node->left.find(r) == node->left.end()
        if (r->op == Operator::none && !isIn) // try only basic cuts
        {
            shared_ptr<ProofNode> newNode1 = childProofNode(node);
            shared_ptr<ProofNode> newNode2 = childProofNode(node);
            newNode1->left.insert(r);
            newNode2->right.insert(r);

            if (!isCycle(newNode1) && !isCycle(newNode2))
            {
                node->leftNode = newNode1;
                node->rightNode = newNode2;
                node->appliedRule = ProofRule::cut;
                if (solve({newNode1, newNode2}))
                {
                    return true;
                }
                break;
            }
        }
    }
    return false;
}

double heuristicVal(shared_ptr<ProofNode> node, Inequality inequality)
{
    bool satisfiedLeft = true;
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
    if (inequality->left.size() == 1 && *inequality->left.begin()->get() == *Relation::get("1"))
    {
        // TODO same for the emmpty side
        satisfiedLeft = true; // TODO: where handle this? empty side equals full
    }
    bool satisfiedRight = true;
    for (auto r2 : inequality->right)
    {
        // TODO right side needs to be emptycurrently
        inequality->right.erase(Relation::get("0")); // TODO: where handle this? empty side equals empty
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
    vector<Inequality> sorted = consHeuristic(node, theory);

    for (auto iequ = sorted.begin(); iequ != sorted.end(); iequ++)
    {
        Inequality inequality = *iequ;
        if (heuristicVal(node, inequality) > 0)
        {
            shared_ptr<ProofNode> newNode1 = childProofNode(node);
            shared_ptr<ProofNode> newNode2 = childProofNode(node);
            if (inequality->right.empty())
            {
                newNode1->left.insert(Relation::get("0"));
            }
            if (inequality->left.empty())
            {
                newNode1->right.insert(Relation::get("1"));
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

            if (!isCycle(newNode1) && !isCycle(newNode2))
            {
                node->leftNode = newNode1;
                node->rightNode = newNode2;
                node->appliedRule = ProofRule::cons;

                if (solve({newNode1, newNode2})) // TODO use
                {
                    return true;
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
        shared_ptr<ProofNode> newNode = childProofNode(node);
        newNode->right.erase(r);

        if (!isCycle(newNode))
        {
            node->leftNode = newNode;
            node->rightNode = nullptr;
            node->appliedRule = ProofRule::weakRight;
            if (solve({newNode}))
            {
                return true;
            }
        }
    }
    return false;
}

bool Solver::weakLeftRule(shared_ptr<ProofNode> node)
{
    for (auto r : node->left)
    {
        shared_ptr<ProofNode> newNode = childProofNode(node);
        newNode->left.erase(r);
        if (!isCycle(newNode))
        {
            node->leftNode = newNode;
            node->rightNode = nullptr;
            node->appliedRule = ProofRule::weakLeft;
            if (solve({newNode}))
            {
                return true;
            }
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
                shared_ptr<ProofNode> newNode1 = childProofNode(node);
                shared_ptr<ProofNode> newNode2 = childProofNode(node);
                newNode1->right = {r2->left};
                newNode2->right = {r2->right};

                if (!isCycle(newNode1) && !isCycle(newNode2))
                {
                    node->leftNode = newNode1;
                    node->rightNode = newNode2;
                    node->appliedRule = ProofRule::loop;
                    if (solve({newNode1, newNode2}))
                    {
                        return true;
                    }
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
        shared_ptr<ProofNode> newNode = childProofNode(node);
        shared_ptr<Relation> rComp = make_shared<Relation>(Operator::composition, r0, r1);
        newNode->left = {rComp, Relation::get("id")};
        newNode->right.clear();

        if (!isCycle(newNode))
        {
            node->leftNode = newNode;
            node->rightNode = nullptr;
            node->appliedRule = ProofRule::empty;
            goals.push(newNode);
            return true;
        }
    }
    return false;
}

bool Solver::idseqEmptyRule(shared_ptr<ProofNode> node)
{
    bool hasId = false;
    for (auto r1 : node->left)
    {
        if (*r1 == *Relation::get("id"))
        {
            for (auto r2 : node->left)
            {
                if (r2->op == Operator::composition)
                {
                    shared_ptr<ProofNode> newNode = childProofNode(node);
                    shared_ptr<Relation> linv = make_shared<Relation>(Operator::inverse, r2->left);
                    newNode->left = {linv, r2->right};
                    newNode->right.clear();

                    if (!isCycle(newNode))
                    {
                        node->leftNode = newNode;
                        node->rightNode = nullptr;
                        node->appliedRule = ProofRule::empty;
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

bool Solver::solve()
{
    shared_ptr<ProofNode> root = goals.top();
    if (Solver::root == nullptr)
    {
        Solver::root = root;
    }
    while (!goals.empty())
    {
        shared_ptr<ProofNode> currentGoal = goals.top();
        if (!silent)
            cout << "* " << currentGoal->relationString() << " | " << currentGoal->appliedRule.toString() << endl;

        // step wise proof
        if (stepwise)
        {
            exportProof(currentGoal);
            cin.ignore();
        }

        if (currentGoal->status == ProofNodeStatus::closed)
        {
            // 1) node is closed
            if (!silent)
                cout << "  pop closed." << endl;
            shared_ptr<ProofNode> provenRule = make_shared<ProofNode>(*currentGoal);
            provenRule->parent = nullptr;
            Solver::proved.insert(provenRule);
            goals.pop();
            continue;
        }
        if (currentGoal->status == ProofNodeStatus::open)
        {
            // 2) node is open (failed): discard node and go on
            if (!silent)
                cout << "  pop failed." << endl;
            currentGoal->leftNode = nullptr;
            currentGoal->rightNode = nullptr;
            currentGoal->appliedRule = ProofRule::none;
            unprovable[currentGoal->currentConsDepth].insert(currentGoal);
            goals.pop();
            continue;
        }

        bool skipGoal = false;
        if (currentGoal->status == ProofNodeStatus::dismiss) // TODO: remove dismiss?
        {
            goals.pop();
            continue;
        }
        // check if is unprovable
        for (auto failed : unprovable[currentGoal->currentConsDepth])
        {
            if (*failed == *currentGoal)
            {
                if (!silent)
                    cout << "Known unprovable goal." << endl;
                currentGoal->status = ProofNodeStatus::dismiss;
                dismissSiblings(currentGoal);
                goals.pop();
                skipGoal = true;
            }
        }
        // check if proven
        for (auto good : Solver::proved)
        {
            if (*good == *currentGoal)
            {
                if (!silent)
                    cout << "Kown proven goal." << endl;
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

        // 3) node has already applied a rule
        if (currentGoal->appliedRule != ProofRule(ProofRule::none))
        {
            bool leftClosed = currentGoal->leftNode == nullptr || currentGoal->leftNode->status == ProofNodeStatus::closed;
            bool leftOpen = currentGoal->leftNode != nullptr && currentGoal->leftNode->status == ProofNodeStatus::open;
            bool rightClosed = currentGoal->rightNode == nullptr || currentGoal->rightNode->status == ProofNodeStatus::closed;
            bool rightOpen = currentGoal->rightNode != nullptr && currentGoal->rightNode->status == ProofNodeStatus::open;
            if (leftClosed && rightClosed)
            {
                if (!silent)
                    cout << "  closed parent" << endl;
                currentGoal->status = ProofNodeStatus::closed;
                continue;
            }
            else if (leftOpen || rightOpen)
            {
                // must do rules, do not try other rule // TODO
                if (currentGoal->appliedRule == ProofRule(ProofRule::andLeft) ||
                    currentGoal->appliedRule == ProofRule(ProofRule::andRight) ||
                    currentGoal->appliedRule == ProofRule(ProofRule::orLeft) ||
                    currentGoal->appliedRule == ProofRule(ProofRule::orRight))
                {
                    if (!silent)
                        cout << "  failed parent rule had to be applicable" << endl;
                    currentGoal->status = ProofNodeStatus::open;
                    dismissSiblings(currentGoal);
                    continue;
                }
                if (!silent)
                    cout << "  try another rule" << endl;
                // try / apply next possible rule for current node
            }
            else
            {
                // must do rules, do not try other rule // TODO
                if (currentGoal->appliedRule == ProofRule(ProofRule::andLeft) ||
                    currentGoal->appliedRule == ProofRule(ProofRule::andRight) ||
                    currentGoal->appliedRule == ProofRule(ProofRule::orLeft) ||
                    currentGoal->appliedRule == ProofRule(ProofRule::orRight))
                {
                    goals.pop();
                    continue;
                }
                if (!silent)
                    cout << "TODO:" << endl;
            }
        }
        // 4) apply rule to node
        // if node has rule applied take next one
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
            done = !done ? seqLeftRule(currentGoal) : done;
        case ProofRule::seqLeft:

            done = !done ? transitiveClosureRule(currentGoal) : done;
        case ProofRule::transitiveClosure:
            // done = !done ? cutRule(currentGoal) : done;
        case ProofRule::cut:
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
            cout << "error" << endl;
            break;
        }

        if (!done)
        {
            if (!silent)
                cout << "  done." << endl;
            // No Rule applicable anymore:
            unprovable[currentGoal->currentConsDepth].insert(currentGoal);
            // TODO old remove: currentGoal->status = ProofNodeStatus::open;
            currentGoal->status = ProofNodeStatus::dismiss; // TODO: remove dismiss?
            // remove unvisited siblings, since solving them does not help
            dismissSiblings(currentGoal);
        }
    }

    exportProof();

    if (Solver::root == root && root->status != ProofNodeStatus::closed)
    {
        // ierative deepening on root Solver
        goals.push(root);
        root->appliedRule = ProofRule::none;
        root->status = ProofNodeStatus::none;
        root->currentConsDepth++;
        cout << "Increase cons bound to " << root->currentConsDepth << endl;
        solve();
    }

    return root->status == ProofNodeStatus::closed;
}

bool Solver::solve(string model1, string model2)
{
    // load models
    load(model1, model2);
    return solve();
}

bool Solver::solve(initializer_list<shared_ptr<ProofNode>> goals)
{
    /*future<bool> future = async(launch::async, [goals, this]() -> bool
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
        subSolver.stepwise = stepwise;
        subSolver.silent = silent;
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
    // hack: concentrate merges double edges when reusing proof nodes, but also merges parent edge
    return "digraph { \nconcentrate=true\nnode [shape=record];\n" + node->toDotFormat(currentGoal) + "}";
}

void Solver::exportProof(shared_ptr<ProofNode> currentGoal)
{
    // export proof
    ofstream dotFile;
    dotFile.open("build/proof.dot");
    dotFile << toDotFormat(root, currentGoal);
    dotFile.close();
}