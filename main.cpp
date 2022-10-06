#include <memory>
#include <iostream>
#include "ProofNode.h"
#include "Solver.h"

using namespace std;

void test(string &name)
{
    Solver solver;
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
        test(s);
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
    shared_ptr<ProofNode> rfe = make_shared<ProofNode>();
    rfe->left = {Relation::get("rfe")};
    rfe->right = {Relation::get("rf")};
    shared_ptr<ProofNode> rmw = make_shared<ProofNode>(); // rmw <= po & RW
    rmw->left = {Relation::get("rmw")};
    rmw->right = {Relation::get("po")};
    shared_ptr<ProofNode> mfence = make_shared<ProofNode>();
    mfence->left = {Relation::get("mfence")};
    mfence->right = {Relation::get("po")};
    shared_ptr<ProofNode> data = make_shared<ProofNode>();
    data->left = {Relation::get("data")};
    data->right = {Relation::get("po-loc")};
    shared_ptr<ProofNode> addr = make_shared<ProofNode>();
    addr->left = {Relation::get("addr")};
    addr->right = {Relation::get("po-loc")};
    shared_ptr<ProofNode> ctrl = make_shared<ProofNode>();
    ctrl->left = {Relation::get("ctrl")};
    ctrl->right = {Relation::get("po-loc")};
    solver.theory = {poloc, rfe, rmw, mfence, data, addr, ctrl};

    cout << "Start Solving..." << endl;
    // TODO: solver.solve("cat/sc.cat", "cat/tso.cat");
    // TODO: solver.solve("cat/tso.cat", "cat/aarch64-modified.cat");
    // TODO: solver.solve("cat/sc.cat", "cat/aarch64-modified.cat");

    // tests();
    string name = "c";
    test(name);
}