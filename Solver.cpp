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

bool Solver::axiomEqual(shared_ptr<ProofNode> node)
{
    cout << node->toDotFormat() << endl;
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
    for (auto r1 : node->left) {
        for (auto r2: node->right) {
            if (*r1 == *r2) {
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
        if (relation->op == Operator::cap) {
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
            newNode->left.erase(relation);
            newNode->left.insert(relation->left);
            newNode->left.insert(relation->right);
            node->leftNode = newNode;
            goals.push(newNode);
            node->appliedRule = "orRight";
            return true;
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
        cout << "Current goal: " << currentGoal << endl;
        goals.pop();
        assert(!currentGoal->closed);

        // one rule application
        bool done;
        done = axiomEqual(currentGoal);
        done = !done ? andLeftRule(currentGoal) : done;
        done = !done ? orRightRule(currentGoal) : done;
        done = !done ? andRightRule(currentGoal) : done;

        if (!done) {
            cout << "No Rule applicable." << endl;
            ofstream dotFile;
            dotFile.open("build/proof.dot");
            dotFile << toDotFormat(root);
            dotFile.close();
            return; 
        }
    }
}

string Solver::toDotFormat(shared_ptr<ProofNode> node)
{
    return "digraph { \nnode [shape=record];\n" + node->toDotFormat() + "}";
}