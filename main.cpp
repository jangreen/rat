#include <memory>
#include "ProofNode.h"
#include "Solver.h"

using namespace std;

int main(int argc, const char *argv[])
{
    // TODO: refactor
    // load theory
    Solver solver;
    shared_ptr<ProofNode> poloc = make_shared<ProofNode>();
    poloc->left = {Relation::get("po-loc")};
    poloc->right = {Relation::get("po")};
    shared_ptr<ProofNode> rfe = make_shared<ProofNode>();
    rfe->left = {Relation::get("rfe")};
    rfe->right = {Relation::get("rf")};
    shared_ptr<ProofNode> rmw = make_shared<ProofNode>(); // rmw <= po & RW
    rmw->left = {Relation::get("rmw")};
    rmw->right = {Relation::get("po")};
    shared_ptr<ProofNode> mfence = make_shared<ProofNode>();
    mfence->left = {Relation::get("mfence")};
    mfence->right = {Relation::get("po")};
    solver.theory = {poloc, rfe, rmw, mfence};

    // SC <= TSO
    solver.solve("cat/sc.cat", "cat/tso.cat");
}