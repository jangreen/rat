#include "Solver.h"
#include <iostream>
#include <fstream>
#include <memory>
#include <algorithm>
#include <iterator>
#include <string>
#include "CatInferVisitor.h"
#include "Constraint.h"

using namespace std;

Solver::Solver() : stepwise(false) {}
Solver::~Solver() {}

bool Solver::isCycle(shared_ptr<ProofNode> node)
{
    // TODO hack: misuse this function to abort too long proof obligatins
    if (node->toDotFormat().length() > 400)
    {
        cout << "TOO COMPLEX NODE." << endl;
        return true;
    }
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
    //------------------------------------
    /*if (node->left.find(Relation::get("0")) != node->left.end())
    {
        node->appliedRule = ProofRule::axiomEmpty;
        node->status = ProofNodeStatus::closed;
        return true;
    }
    if (node->right.find(Relation::get("1")) != node->left.end())
    {
        node->appliedRule = ProofRule::axiomFull;
        node->status = ProofNodeStatus::closed;
        return true;
    }
    if (node->left.size() == 1 && node->left == node->right)
    {
        node->appliedRule = ProofRule::axiomEqual;
        node->status = ProofNodeStatus::closed;
        return true;
    }
    for (auto inequality : theory)
    {
        if (node->left == inequality->left && node->right == inequality->right)
        {
            node->appliedRule = ProofRule::axiomTheory;
            node->status = ProofNodeStatus::closed;
            return true;
        }
    }
    return false;*/
    //---------------------------------------
    for (auto r1 : node->left)
    {
        if (*r1 == *Relation::EMPTY)
        {
            node->appliedRule = ProofRule::axiomEmpty;
            node->status = ProofNodeStatus::closed;
            return true;
        }

        for (auto r2 : node->right)
        {
            if (*r2 == *Relation::FULL)
            {
                node->appliedRule = ProofRule::axiomFull;
                node->status = ProofNodeStatus::closed;
                return true;
            }

            if (*r1 == *r2)
            {
                node->appliedRule = ProofRule::axiomEqual;
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
                            node->appliedRule = ProofRule::axiomTheory;
                            node->status = ProofNodeStatus::closed;
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
            { // TODO: must rules
                // must rule: if this leads to cycle stop
                node->status = ProofNodeStatus::open;
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
            { // TODO: must rules
                // must rule: if this leads to cycle stop
                node->status = ProofNodeStatus::open;
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
            { // TODO: must rules
                // must rule: if this leads to cycle stop
                node->status = ProofNodeStatus::open;
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
            { // TODO: must rules
                // must rule: if this leads to cycle stop
                node->status = ProofNodeStatus::open;
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
                seqSolver.stepwise = stepwise;
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
                    newNode1->parent = node;
                    node->rightNode = newNode2;
                    node->appliedRule = ProofRule::seqLeft;
                    return true;
                }
            }
        }
    }
    /*/ TODO: append id
    for (auto r1 : node->left)
    {
        Solver seqSolver;
        seqSolver.theory = theory;
        seqSolver.stepwise = stepwise;
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

                Solver tcSolver;
                tcSolver.theory = theory;
                tcSolver.stepwise = stepwise;
                tcSolver.goals.push(newNode);
                if (tcSolver.solve())
                {
                    // successful tc
                    node->leftNode = newNode;
                    node->rightNode = nullptr;
                    node->appliedRule = ProofRule::transitiveClosure;
                    return true;
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

            cout << newNode1->toDotFormat() << endl;
            cout << newNode2->toDotFormat() << endl;

            Solver cutSolver;
            cutSolver.theory = theory;
            cutSolver.stepwise = stepwise;
            cutSolver.goals.push(newNode1);
            cutSolver.goals.push(newNode2);
            if (cutSolver.solve())
            {
                // successful cut
                node->leftNode = newNode1;
                newNode1->parent = node;
                node->rightNode = newNode2;
                node->appliedRule = ProofRule::cut;
                return true;
            }
            break;
        }
    }
    return false;
}

bool Solver::consRule(shared_ptr<ProofNode> node)
{
    for (auto inequality : theory)
    {
        // old cons checks if applicable
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
        if (satisfiedLeft)
        {
            shared_ptr<ProofNode> newNode = childProofNode(node);
            for (auto n : inequality->right)
            { // TODO: wrong -> do case split
                newNode->left.insert(n);
            }

            /*Solver consSolver;
            consSolver.theory = theory;
            consSolver.stepwise = stepwise;
            consSolver.goals.push(newNode);*/
            if (!isCycle(newNode)) //! isCycle(newNode)
            {
                node->leftNode = newNode;
                node->rightNode = nullptr;
                node->appliedRule = ProofRule::cons;
                goals.push(newNode);
                return true;
            }
        }
        // old cons checks if applicable
        bool satisfiedRight = true;
        for (auto r2 : inequality->right)
        {
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
        if (satisfiedRight)
        {
            shared_ptr<ProofNode> newNode = childProofNode(node);
            for (auto n : inequality->left)
            { // TODO: wrong -> do case split
                newNode->right.insert(n);
            }

            /*Solver consSolver;
            consSolver.theory = theory;
            consSolver.stepwise = stepwise;
            consSolver.goals.push(newNode);*/
            if (!isCycle(newNode)) // consolver.solve() // TODO which is better?
            {
                node->leftNode = newNode;
                node->rightNode = nullptr;
                node->appliedRule = ProofRule::cons;
                goals.push(newNode);
                return true;
            }
        }
        /* new cons does not check, maybe use old as heuristic?
        shared_ptr<ProofNode> newNode1 = childProofNode(node);
        shared_ptr<ProofNode> newNode2 = childProofNode(node);
        for (auto n : inequality->right)
        {
            newNode1->left.insert(n);
        }
        for (auto n : inequality->left)
        {
            newNode2->right.insert(n);
        }

        if (!isCycle(newNode1) && !isCycle(newNode2))
        {
            node->leftNode = newNode1;
            node->rightNode = newNode2;
            node->appliedRule = ProofRule::cons;
            goals.push(newNode1);
            goals.push(newNode2);
            return true;
        }*/
    }
    return false;
}

bool Solver::weakRightRule(shared_ptr<ProofNode> node)
{
    for (auto r : node->right)
    {
        Solver weakRightSolver;
        weakRightSolver.theory = theory;
        weakRightSolver.stepwise = stepwise;

        shared_ptr<ProofNode> newNode = childProofNode(node);
        newNode->right.erase(r);
        weakRightSolver.goals.push(newNode);
        if (weakRightSolver.solve())
        {
            // successful weakening
            node->leftNode = newNode;
            node->rightNode = nullptr;
            node->appliedRule = ProofRule::weakRight;
            return true;
        }
    }
    return false;
}

bool Solver::weakLeftRule(shared_ptr<ProofNode> node)
{
    for (auto r : node->left)
    {
        Solver weakLeftSolver;
        weakLeftSolver.theory = theory;
        weakLeftSolver.stepwise = stepwise;

        shared_ptr<ProofNode> newNode = childProofNode(node);
        newNode->left.erase(r);
        weakLeftSolver.goals.push(newNode);
        if (weakLeftSolver.solve())
        {
            // successful weakening
            node->leftNode = newNode;
            node->rightNode = nullptr;
            node->appliedRule = ProofRule::weakLeft;
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
                loopSolver.stepwise = stepwise;
                loopSolver.goals.push(newNode1);
                loopSolver.goals.push(newNode2);
                if (loopSolver.solve())
                {
                    // successful split
                    node->leftNode = newNode1;
                    node->rightNode = newNode2;
                    node->appliedRule = ProofRule::loop;
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
        shared_ptr<ProofNode> newNode = make_shared<ProofNode>();
        shared_ptr<Relation> rComp = make_shared<Relation>(Operator::composition, r0, r1);
        newNode->left.insert(rComp);
        newNode->left.insert(Relation::get("id"));

        node->leftNode = newNode;
        node->rightNode = nullptr;
        node->appliedRule = ProofRule::empty;
        goals.push(newNode);
        return true;
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
    while (!goals.empty())
    {
        shared_ptr<ProofNode> currentGoal = goals.top();

        // step wise proof
        if (stepwise)
        {
            exportProof(root);
            cin.get();
        }

        if (currentGoal->status == ProofNodeStatus::closed)
        {
            // 1) node is closed
            cout << "Popped closed goal." << endl;
            goals.pop();
            continue;
        }
        if (currentGoal->status == ProofNodeStatus::open)
        {
            // 2) node is open (failed): discard node and go on
            cout << "Popped failed goal." << endl;
            currentGoal->leftNode = nullptr;
            currentGoal->rightNode = nullptr;
            currentGoal->appliedRule = ProofRule::none;
            unprovable.insert(currentGoal);
            goals.pop();
            continue;
        }
        // check if is unprovable
        for (auto failed : unprovable)
        {
            if (*failed == *currentGoal)
            {
                cout << "Known unprovable goal popped." << endl;
                goals.pop();
                continue;
            }
        }

        // 3) node has already applied a rule
        if (currentGoal->appliedRule != ProofRule(ProofRule::none))
        {
            bool leftClosed = currentGoal->leftNode == nullptr || currentGoal->leftNode->status == ProofNodeStatus::closed;
            bool leftOpen = currentGoal->leftNode == nullptr || currentGoal->leftNode->status == ProofNodeStatus::open;
            bool rightClosed = currentGoal->rightNode == nullptr || currentGoal->rightNode->status == ProofNodeStatus::closed;
            bool rightOpen = currentGoal->rightNode == nullptr || currentGoal->rightNode->status == ProofNodeStatus::open;
            if (leftClosed && rightClosed)
            {
                cout << "Closed parent goal." << endl;
                currentGoal->status = ProofNodeStatus::closed;
                continue;
            }
            else if (leftOpen || rightOpen)
            {
                cout << "Try another rule" << endl;
                // try / apply next possible rule for current node
            }
            else
            {
                cout << "TODO: does this happen?" << endl;
            }
        }
        // 4) apply rule to node
        // if node has rule applied take next one
        bool done = false;
        switch (currentGoal->appliedRule)
        {
        case ProofRule::none:
            done = axiom(currentGoal);
        case ProofRule::axiomEmpty:
        case ProofRule::axiomFull:
        case ProofRule::axiomEqual:
        case ProofRule::axiomTheory:
            done = !done ? andLeftRule(currentGoal) : done;
        case ProofRule::andLeft:
            done = !done ? orRightRule(currentGoal) : done;
        case ProofRule::orRight:
            done = !done ? andRightRule(currentGoal) : done;
        case ProofRule::andRight:
            done = !done ? orLeftRule(currentGoal) : done;
        case ProofRule::orLeft:
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
            done = !done ? loopRule(currentGoal) : done;
        case ProofRule::loop:
            done = !done ? consRule(currentGoal) : done;
        case ProofRule::cons:
            // done = !done ? weakRightRule(currentGoal) : done;
        case ProofRule::weakRight:
            // done = !done ? weakLeftRule(currentGoal) : done;
        case ProofRule::weakLeft:
            done = !done ? invcapEmptyRule(currentGoal) : done;
        case ProofRule::empty:
            break;
        default:
            cout << "error" << endl;
            break;
        }

        if (!done)
        {
            // No Rule applicable anymore:
            currentGoal->status = ProofNodeStatus::open;
            // remove unvisited siblings, since solving them does not help
            // TODO: temp mark them as open, if open goals are saved during the proof do not use this
            if (currentGoal->parent->leftNode != nullptr && currentGoal->parent->leftNode->status == ProofNodeStatus::none)
            {
                currentGoal->parent->leftNode->status = ProofNodeStatus::open;
            }
            if (currentGoal->parent->rightNode != nullptr && currentGoal->parent->rightNode->status == ProofNodeStatus::none)
            {
                currentGoal->parent->rightNode->status = ProofNodeStatus::open;
            }
        }
    }

    exportProof(root);

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

void Solver::exportProof(shared_ptr<ProofNode> root)
{
    // export proof
    ofstream dotFile;
    dotFile.open("build/proof.dot");
    dotFile << toDotFormat(root);
    dotFile.close();
}