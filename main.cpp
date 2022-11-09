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
    // TODO: make shared <Ineq>
    Inequality wrwr0 = make_shared<ProofNode>("W*R;W*R", "0");
    // Inequality wrww0 = make_shared<ProofNode>("W*R;W*W", "0");
    // Inequality wwrr0 = make_shared<ProofNode>("W*W;R*R", "0");
    // Inequality wwrw0 = make_shared<ProofNode>("W*W;R*W", "0");
    // Inequality rwrr0 = make_shared<ProofNode>("R*W;R*R", "0");
    // ... TODO
    Inequality popo = make_shared<ProofNode>("po;po", "po");
    Inequality poid0 = make_shared<ProofNode>("po & id", "0");
    Inequality poloc1 = make_shared<ProofNode>("po-loc", "po & loc");
    Inequality poloc2 = make_shared<ProofNode>("po & loc", "po-loc");
    Inequality rf = make_shared<ProofNode>("rf", "W*R & loc");
    Inequality rfe = make_shared<ProofNode>("rfe", "rf & ext");
    Inequality erf = make_shared<ProofNode>("rf & ext", "rfe");
    Inequality coe = make_shared<ProofNode>("coe", "co & ext");
    Inequality locloc = make_shared<ProofNode>("loc;loc", "loc");
    Inequality idloc = make_shared<ProofNode>("id & M*M", "loc");
    Inequality locinv = make_shared<ProofNode>("loc", "loc^-1");
    Inequality invloc = make_shared<ProofNode>("loc^-1", "loc");
    Inequality intext = make_shared<ProofNode>("1", "int | ext");
    Inequality int1 = make_shared<ProofNode>("int", "po | po^-1 | id");
    Inequality int2 = make_shared<ProofNode>("po | po^-1 | id", "int");
    Inequality rmw = make_shared<ProofNode>("rmw", "po & R*W");
    Inequality mfence = make_shared<ProofNode>("mfence", "po");
    Inequality data = make_shared<ProofNode>("data", "po & R*M");
    Inequality ctrl = make_shared<ProofNode>("ctrl", "po & R*M");
    Inequality addr = make_shared<ProofNode>("addr", "po & R*M");
    Inequality scRmw = make_shared<ProofNode>("rmw", "0");
    solver.theory = {wrwr0, popo, poid0, poloc1, poloc2, rf, rfe, erf, coe, locloc, idloc, locinv, invloc, intext, int1, int2, rmw, mfence, data, ctrl, addr, scRmw}; // TODO scRmw needed?
}

int main(int argc, const char *argv[])
{
    Inequality uniproc = make_shared<ProofNode>("(po-loc | co | fr | rf)^+ & id", "0");
    Solver solver;
    loadTheory(solver);
    // solver.stepwise = true;
    // solver.silent = false;

    cout << "Start Solving..." << endl;
    /* cout << "* SC <= OOTA:\n"
         << solver.solve("cat/sc.cat", "cat/oota.cat") << endl; // */
    cout << " * SC <= TSO:\n"
         << solver.solve("cat/sc.cat", "cat/tso.cat") << endl; // */
    /*solver.theory.insert(uniproc);
    solver.solve({make_shared<ProofNode>("rf & int", "po-loc")}); // */
    // solver.solve("test/uniproc1.cat", "test/uniproc2.cat"); // model is easier than '<= po under uniproc'

    /* cout << "* Split reads-from:\n"
         << solver.solve("test/split1.cat", "test/split2.cat") << endl; // */
    // solver.solve("test/uniproc121.cat", "test/uniproc221.cat");
    // solver.solve({make_shared<ProofNode>("(po-loc;data) & id", "po-loc")});
    /* solver.theory.insert(uniproc);
    solver.solve({make_shared<ProofNode>("((rf & int);data) & id", "0")}); //+ // */
    /* cout << "* rf+ <= rf:\n"
         << solver.solve({make_shared<ProofNode>("rf+", "rf")}) << endl; // */
    // solver.solve("cat/tso-modified.cat", "cat/oota.cat");

    // solver.solve({make_shared<ProofNode>("po-loc;po-loc", "po-loc")});

    // TODO: solver.solve("cat/tso.cat", "cat/aarch64-modified.cat");
    // solver.solve("cat/sc.cat", "cat/aarch64-modified.cat");

    // TODO: refactor: tests();
    /*string name = "d";
    Solver testSolver;
    shared_ptr<ProofNode> ab = make_shared<ProofNode>();
    ab->left = {Relation::get("a")};
    ab->right = {Relation::get("b")};
    shared_ptr<ProofNode> bc = make_shared<ProofNode>();
    bc->left = {Relation::get("b")};
    bc->right = {Relation::get("c")};
    testSolver.theory = {ab, bc};*/

    // test(testSolver, name);

    // solver.solve("test/lemma1.cat", "test/lemma2.cat");
    /*solver.solve("test/unlemma1.cat", "test/unlemma2.cat");
    solver.solve("test/unlemma1.cat", "test/unlemma2.cat");*/
}