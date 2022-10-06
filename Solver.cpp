#include <iostream>
#include <fstream>
#include <memory>
#include <algorithm>
#include <iterator>
#include <string>
#include "Solver.h"
#include "ProofNode.h"

using namespace std;

Solver::Solver() {}
Solver::~Solver() {}

bool Solver::axiom(shared_ptr<ProofNode> node)
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
        if (*r1 == *Relation::EMPTY)
        {
            node->appliedRule = "axiomEmpty";
            node->status = ProofNodeStatus::closed;
            return true;
        }

        for (auto r2 : node->right)
        {
            if (*r2 == *Relation::FULL)
            {
                node->appliedRule = "axiomFull";
                node->status = ProofNodeStatus::closed;
                return true;
            }

            if (*r1 == *r2)
            {
                node->appliedRule = "axiomEqual";
                node->status = ProofNodeStatus::closed;
                return true;
            }

            for (auto inequality : theory) // support only for simple theories
            {
                for (auto l1 : inequality->left)
                {
                    for (auto l2 : inequality->right)
                    {
                        if (*l1 == *r1 && *l2 == *r2) // check theory
                        {
                            node->appliedRule = "axiomTheory";
                            node->status = ProofNodeStatus::closed;
                            goals.pop();
                            return true;
                        }
                    }
                }
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
            shared_ptr<ProofNode> newNode = make_shared<ProofNode>(*node);
            newNode->left.erase(relation);
            newNode->left.insert(relation->left);
            newNode->left.insert(relation->right);
            node->leftNode = newNode;
            goals.push(newNode);
            node->appliedRule = "andLeft";
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
            shared_ptr<ProofNode> newNode1 = make_shared<ProofNode>(*node);
            shared_ptr<ProofNode> newNode2 = make_shared<ProofNode>(*node);
            newNode1->right.erase(relation);
            newNode2->right.erase(relation);
            newNode1->right.insert(relation->left);
            newNode2->right.insert(relation->right);
            node->leftNode = newNode1;
            node->rightNode = newNode2;
            goals.push(newNode1);
            goals.push(newNode2);
            node->appliedRule = "andRight";
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
            shared_ptr<ProofNode> newNode = make_shared<ProofNode>(*node);
            newNode->right.erase(relation);
            newNode->right.insert(relation->left);
            newNode->right.insert(relation->right);
            node->leftNode = newNode;
            goals.push(newNode);
            node->appliedRule = "orRight";
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
            shared_ptr<ProofNode> newNode1 = make_shared<ProofNode>(*node);
            shared_ptr<ProofNode> newNode2 = make_shared<ProofNode>(*node);
            newNode1->left.erase(relation);
            newNode2->left.erase(relation);
            newNode1->left.insert(relation->left);
            newNode2->left.insert(relation->right);
            node->leftNode = newNode1;
            node->rightNode = newNode2;
            goals.push(newNode1);
            goals.push(newNode2);
            node->appliedRule = "orLeft";
            return true;
        }
    }
    return false;
}

bool Solver::transitiveClosureRule(shared_ptr<ProofNode> node)
{
    for (auto r1 : node->left)
    {
        for (auto r2 : node->right)
        {
            // simple case both have transitive closure
            if (r1->op == Operator::transitive && r2->op == Operator::transitive)
            {
                shared_ptr<ProofNode> newNode = make_shared<ProofNode>();
                newNode->left.insert(r1->left);
                newNode->right.insert(r2->left);
                node->leftNode = newNode;
                node->appliedRule = "transitiveClosureRule";
                goals.push(newNode);
                return true;
            }
        }
    }
    return false;
}

void Solver::solve()
{
    cout << "Start Solving." << endl;
    shared_ptr<ProofNode> root = goals.top();
    while (!goals.empty())
    {
        shared_ptr<ProofNode> currentGoal = goals.top();

        if (currentGoal->status != ProofNodeStatus::none)
        {
            // node is other closed or open (failed), go on
            goals.pop();
            continue;
        }
        if (!currentGoal->appliedRule.empty()) // TODO: refactor
        {
            bool leftClosed = currentGoal->leftNode == nullptr || currentGoal->leftNode->status == ProofNodeStatus::closed;
            bool leftOpen = currentGoal->leftNode == nullptr || currentGoal->leftNode->status == ProofNodeStatus::open;
            bool rightClosed = currentGoal->rightNode == nullptr || currentGoal->rightNode->status == ProofNodeStatus::closed;
            bool rightOpen = currentGoal->rightNode == nullptr || currentGoal->rightNode->status == ProofNodeStatus::open;
            if (leftClosed && rightClosed)
            {
                currentGoal->status = ProofNodeStatus::closed;
                goals.pop();
                continue;
            }
            else if (leftOpen || rightOpen)
            {
                currentGoal->status = ProofNodeStatus::open;
                goals.pop();
                continue;
            }
        }

        // one rule application
        bool done;
        done = axiom(currentGoal);
        done = !done ? andLeftRule(currentGoal) : done;
        done = !done ? orRightRule(currentGoal) : done;
        done = !done ? andRightRule(currentGoal) : done;
        done = !done ? orLeftRule(currentGoal) : done;
        done = !done ? transitiveClosureRule(currentGoal) : done;

        if (!done)
        {
            // No Rule applicable: pop goal on go on
            cout << "Open goal: " << currentGoal->toDotFormat() << endl;
            currentGoal->status = ProofNodeStatus::open;
            goals.pop();
        }
    }

    // export proof
    ofstream dotFile;
    dotFile.open("build/proof.dot");
    dotFile << toDotFormat(root);
    dotFile.close();
}

string Solver::toDotFormat(shared_ptr<ProofNode> node)
{
    return "digraph { \nnode [shape=record];\n" + node->toDotFormat() + "}";
}