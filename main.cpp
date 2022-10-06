#include <memory>
#include "CatInferVisitor.h"
#include "Constraint.h"
#include "ProofNode.h"
#include "Solver.h"

using namespace std;

int main(int argc, const char *argv[])
{
    shared_ptr<ProofNode> goal = make_shared<ProofNode>();
    CatInferVisitor visitor;

    // SC <= TSO
    // first program
    ConstraintSet sc = visitor.parse("cat/sc.cat");
    for (auto &[name, constraint] : sc)
    {
        constraint.toEmptyNormalForm();
        goal->right.insert(constraint.relation);
    }

    // second program
    ConstraintSet tso = visitor.parse("cat/tso.cat");
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

    Solver solver;
    shared_ptr<ProofNode> poloc = make_shared<ProofNode>();
    poloc->left = {Relation::get("po-loc")};
    poloc->right = {Relation::get("po")};
    shared_ptr<ProofNode> rfe = make_shared<ProofNode>();
    rfe->left = {Relation::get("rfe")};
    rfe->right = {Relation::get("rf")};
    solver.theory = {poloc, rfe};
    solver.goals.push(goal);
    solver.solve();
}