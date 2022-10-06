#include <iostream>
#include <fstream>
#include <memory>
#include <algorithm>
#include <iterator>
#include <string>
#include "Solver.h"
#include "ProofNode.h"
#include "CatInferVisitor.h"
#include "Constraint.h"

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

bool Solver::seqLeftRule(shared_ptr<ProofNode> node)
{
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
                Solver seqSolver;
                seqSolver.theory = theory;
                shared_ptr<ProofNode> newNode1 = make_shared<ProofNode>();
                shared_ptr<ProofNode> newNode2 = make_shared<ProofNode>();
                newNode1->left.insert(r1->left);
                newNode1->right.insert(r2->left);
                newNode2->left.insert(r1->right);
                newNode2->right.insert(r2->right);
                seqSolver.goals.push(newNode1);
                seqSolver.goals.push(newNode2);
                if (seqSolver.solve())
                {
                    // successful split
                    node->leftNode = newNode1;
                    node->rightNode = newNode2;
                    node->appliedRule = "seqLeftRule";
                    return true;
                }
            }
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

bool Solver::unrollRule(shared_ptr<ProofNode> node)
{
    for (auto r : node->right)
    {
        if (r->op == Operator::transitive)
        {
            shared_ptr<ProofNode> newNode = make_shared<ProofNode>(*node);
            newNode->right.erase(r);
            newNode->right.insert(r->left);
            node->leftNode = newNode;
            node->appliedRule = "unrollRule";
            goals.push(newNode);
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
                shared_ptr<ProofNode> newNode1 = make_shared<ProofNode>();
                shared_ptr<ProofNode> newNode2 = make_shared<ProofNode>();
                newNode1->left = node->left;
                newNode1->right.insert(r2->left);
                newNode2->left = node->left;
                newNode2->right.insert(r2->right);

                Solver loopSolver;
                loopSolver.theory = theory;
                loopSolver.goals.push(newNode1);
                loopSolver.goals.push(newNode2);
                if (loopSolver.solve())
                {
                    // successful split
                    node->leftNode = newNode1;
                    node->rightNode = newNode2;
                    node->appliedRule = "loopRule";
                    return true;
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
    for (auto &[name, constraint] : sc)
    {
        constraint.toEmptyNormalForm();
        goal->right.insert(constraint.relation);
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
        done = !done ? seqLeftRule(currentGoal) : done;
        done = !done ? transitiveClosureRule(currentGoal) : done;
        done = !done ? unrollRule(currentGoal) : done;
        done = !done ? loopRule(currentGoal) : done;

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

    return root->status == ProofNodeStatus::closed;
}

bool Solver::solve(string model1, string model2)
{
    // load models
    load(model1, model2);
    return solve();
}

string Solver::toDotFormat(shared_ptr<ProofNode> node)
{
    return "digraph { \nnode [shape=record];\n" + node->toDotFormat() + "}";
}