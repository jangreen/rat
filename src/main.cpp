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
    // setup solvers
    Solver solver;
    loadTheory(solver);
    Inequality uniproc = make_shared<ProofNode>("(po-loc | co | fr | rf)^+ & id", "0");
    Solver uniprocSolver;
    loadTheory(uniprocSolver);
    uniprocSolver.theory.insert(uniproc);
    uniprocSolver.logLevel = 1;

    // solver.solve("cat/sc.cat", "cat/oota.cat");
    // solver.exportProof("SC<=OOTA");
    // solver.solve("cat/sc.cat", "cat/tso.cat");
    // solver.exportProof("SC<=TSO");
    // uniprocSolver.stepwise = true;
    uniprocSolver.solve(make_shared<ProofNode>("rf & int", "po-loc"));
    uniprocSolver.exportProof("rfi<=po-loc+Uniproc");
    cout << Solver::iterations << " iter" << endl;
    // solver.solve("cat/tso-modified.cat", "cat/oota.cat");
    // solver.exportProof("TSO<=OOTA");
}