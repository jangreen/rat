#include "CatInferVisitor.h"
#include "Constraint.h"
#include "ProofObligation.h"

using namespace std;

int main(int argc, const char *argv[]) {
    ProofObligation goal;
    CatInferVisitor visitor;

    // SC <= TSO
    // first program
    ConstraintSet sc = visitor.parse("cat/sc.cat");
    for (auto &[name, constraint] : sc) {
        std::cout << name << std::endl;
        //std::cout << constraint.relation->description() << std::endl;

        // Constraint normalFormed = constraint.emptyNormalForm();
        // Relation r = *normalFormed.relation;
        // std::cout << r.empty->name << std::endl;
        // goal.right.insert(*constraint.relation);
    }

    // // second program
    // ConstraintSet tso = visitor.parse("cat/tso.cat");
    // for (const auto &[name, Constraint] : tso)
    // {
    //     std::cout << name << std::endl;
    // }
}