#include <memory>
#include <iostream>
#include "ProofNode.h"
#include "Solver.h"

using namespace std;

void test(Solver &solver, string &name)
{
    if (!solver.solve("test/" + name + "1.cat", "test/" + name + "2.cat"))
    {
        cout << name << " failed." << endl;
    }
}
void tests()
{
    cout << "Tests..." << endl;
    for (string s : {"a", "b"})
    {
        Solver solver;
        test(solver, s);
    }
    cout << "Done." << endl;
}

int main(int argc, const char *argv[])
{
    // TODO: refactor
    // load theory
    Solver solver;
    shared_ptr<ProofNode> poloc = make_shared<ProofNode>();
    poloc->left = {Relation::get("po-loc")};
    poloc->right = {Relation::get("po")};
    shared_ptr<ProofNode> rf = make_shared<ProofNode>();
    rf->left = {Relation::get("rf")};
    rf->right = {Relation::get("W*R")};
    shared_ptr<ProofNode> rf2 = make_shared<ProofNode>();
    rf2->left = {Relation::get("rf")};
    rf2->right = {Relation::get("loc")};
    shared_ptr<ProofNode> rfe = make_shared<ProofNode>();
    rfe->left = {Relation::get("rfe")};
    rfe->right = {Relation::get("rf")};
    shared_ptr<ProofNode> rmw = make_shared<ProofNode>(); // rmw <= po & RW
    rmw->left = {Relation::get("rmw")};
    rmw->right = {Relation::get("po")};
    shared_ptr<ProofNode> mfence = make_shared<ProofNode>();
    mfence->left = {Relation::get("mfence")};
    mfence->right = {Relation::get("po")};
    shared_ptr<ProofNode> data1 = make_shared<ProofNode>();
    data1->left = {Relation::get("data")};
    data1->right = {Relation::get("po")};
    shared_ptr<ProofNode> addr1 = make_shared<ProofNode>();
    addr1->left = {Relation::get("addr")};
    addr1->right = {Relation::get("po")};
    shared_ptr<ProofNode> ctrl1 = make_shared<ProofNode>();
    ctrl1->left = {Relation::get("ctrl")};
    ctrl1->right = {Relation::get("po")};
    shared_ptr<ProofNode> data2 = make_shared<ProofNode>();
    data2->left = {Relation::get("data")};
    data2->right = {Relation::get("R*M")};
    shared_ptr<ProofNode> addr2 = make_shared<ProofNode>();
    addr2->left = {Relation::get("addr")};
    addr2->right = {Relation::get("R*M")};
    shared_ptr<ProofNode> ctrl2 = make_shared<ProofNode>();
    ctrl2->left = {Relation::get("ctrl")};
    ctrl2->right = {Relation::get("R*M")};
    solver.theory = {poloc, rf, rf2, rfe, rmw, mfence, data1, addr1, ctrl1, data2, addr2, ctrl2};

    cout << "Start Solving..." << endl;
    // solver.solve("cat/sc.cat", "cat/tso.cat");

    solver.solve("test/uniproc1.cat", "test/uniproc2.cat");
    // solver.solve("cat/sc.cat", "cat/oota.cat");
    // solver.solve("cat/tso.cat", "cat/oota.cat");

    // TODO: solver.solve("cat/tso.cat", "cat/aarch64-modified.cat");
    // TODO: solver.solve("cat/sc.cat", "cat/aarch64-modified.cat");

    // tests();
    string name = "d";
    Solver testSolver;
    shared_ptr<ProofNode> ab = make_shared<ProofNode>();
    ab->left = {Relation::get("a")};
    ab->right = {Relation::get("b")};
    shared_ptr<ProofNode> bc = make_shared<ProofNode>();
    bc->left = {Relation::get("b")};
    bc->right = {Relation::get("c")};
    testSolver.theory = {ab, bc};

    // test(testSolver, name);
}