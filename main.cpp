#include <memory>
#include <iostream>
#include "Solver.h"
#include "ProofNode.h"

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

void loadTheory(Solver &solver)
{
    Inequality popo = make_shared<ProofNode>("po;po", "po");
    Inequality poloc1 = make_shared<ProofNode>("po-loc", "po & loc");
    Inequality poloc2 = make_shared<ProofNode>("po & loc", "po-loc");
    Inequality rf = make_shared<ProofNode>("rf", "W*R & loc");
    Inequality rfe = make_shared<ProofNode>("rfe", "rf & ext");
    Inequality coe = make_shared<ProofNode>("coe", "co & ext");
    Inequality locloc = make_shared<ProofNode>("loc;loc", "loc");
    Inequality idloc = make_shared<ProofNode>("id", "loc");
    Inequality intext = make_shared<ProofNode>("1", "int | ext");
    Inequality int1 = make_shared<ProofNode>("int", "po | po^-1 | id");
    Inequality int2 = make_shared<ProofNode>("po | po^-1 | id", "int");
    Inequality rmw = make_shared<ProofNode>("rmw", "po & R*W");
    Inequality mfence = make_shared<ProofNode>("mfence", "po");
    Inequality data = make_shared<ProofNode>("data", "po & R*M");
    Inequality ctrl = make_shared<ProofNode>("ctrl", "po & R*M");
    Inequality addr = make_shared<ProofNode>("addr", "po & R*M");
    Inequality scRmw = make_shared<ProofNode>("rmw", "0");
    solver.theory = {popo, poloc1, poloc2, rf, rfe, coe, locloc, idloc, intext, int1, int2, rmw, mfence, data, ctrl, addr, scRmw}; // TODO scRmw needed?
}

int main(int argc, const char *argv[])
{
    Solver solver;
    loadTheory(solver);
    // solver.stepwise = true;
    solver.silent = true;

    cout << "Start Solving..." << endl;
    // solver.solve("cat/sc.cat", "cat/tso.cat");
    // solver.solve("test/uniproc1.cat", "test/uniproc2.cat");
    // solver.solve("cat/sc.cat", "cat/oota.cat");

    // solver.solve("test/uniproc121.cat", "test/uniproc221.cat");
    solver.solve("test/uniproc12.cat", "test/uniproc22.cat");
    // solver.solve("cat/tso-modified.cat", "cat/oota.cat");

    // TODO: solver.solve("cat/tso.cat", "cat/aarch64-modified.cat");
    // solver.solve("cat/sc.cat", "cat/aarch64-modified.cat");

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

    // solver.solve("test/lemma1.cat", "test/lemma2.cat");
    /*solver.solve("test/unlemma1.cat", "test/unlemma2.cat");
    solver.solve("test/unlemma1.cat", "test/unlemma2.cat");*/
}