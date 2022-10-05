#include "CatInferVisitor.h"
#include "Constraint.h"
#include "ProofObligation.h"

using namespace std;

int main(int argc, const char *argv[])
{
    ProofObligation goal;
    CatInferVisitor visitor;

    // SC <= TSO
    // first program
    ConstraintSet sc = visitor.parse("cat/sc.cat");
    for (auto &[name, constraint] : sc)
    {
        constraint.toEmptyNormalForm();
        goal.right.insert(constraint.relation);
    }

    // second program
    ConstraintSet tso = visitor.parse("cat/tso.cat");
    for (auto &[name, constraint] : tso)
    {
        constraint.toEmptyNormalForm();
        goal.left.insert(constraint.relation);
    }
}