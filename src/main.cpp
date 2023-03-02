#include <memory>
#include <iostream>
#include <fstream>
#include <vector>
#include "Relation.h"
#include "Tableau.h"
#include "RegularTableau.h"
// #include "Solver.h"
// #include "ProofNode.h"

using namespace std;

/* void loadTheory(Solver &solver)
{
    // TODO: make shared <Ineq>
    Inequality wrwr0 = make_shared<ProofNode>("W*R;W*R", "0");
    Inequality wrww0 = make_shared<ProofNode>("W*R;W*W", "0");
    // Inequality wwrr0 = make_shared<ProofNode>("W*W;R*R", "0");
    // Inequality wwrw0 = make_shared<ProofNode>("W*W;R*W", "0");
    // Inequality rwrr0 = make_shared<ProofNode>("R*W;R*R", "0");
    Inequality rmwr_rr = make_shared<ProofNode>("R*M;W*R", "R*R");
    Inequality rr_rm = make_shared<ProofNode>("R*R", "R*M");
    Inequality rmrm = make_shared<ProofNode>("R*M;R*M", "R*M");

    // ... TODO
    Inequality popo = make_shared<ProofNode>("po;po", "po");
    Inequality poid0 = make_shared<ProofNode>("po & id", "0");
    Inequality poloc1 = make_shared<ProofNode>("po-loc", "po & loc");
    Inequality poloc2 = make_shared<ProofNode>("po & loc", "po-loc");
    Inequality rf = make_shared<ProofNode>("rf", "W*R & loc");
    Inequality rfrf1 = make_shared<ProofNode>("rf;rf^-1", "id");
    Inequality rfe = make_shared<ProofNode>("rfe", "rf & ext");
    Inequality erf = make_shared<ProofNode>("rf & ext", "rfe");
    Inequality co = make_shared<ProofNode>("co", "W*W & loc");
    Inequality coco = make_shared<ProofNode>("co;co", "co");
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
    solver.theory = {wrwr0, wrww0, rmwr_rr, rr_rm, rmrm, popo, poid0, poloc1, poloc2, rf, rfrf1, rfe, erf, co, coco, coe, locloc, idloc, locinv, invloc, intext, int1, int2, rmw, mfence, data, ctrl, addr, scRmw}; // TODO scRmw needed?
}//*/

int main(int argc, const char *argv[])
{

    shared_ptr<Relation> r1 = Relation::parse("(id;a)^*");
    shared_ptr<Relation> r2 = Relation::parse("a;(a;a)^* | (a;a)^*");
    r1->label = 0;
    r1->negated = false;
    r2->label = 0;
    r2->negated = true;

    Tableau tableau{r1, r2};
    tableau.solve(100);
    tableau.exportProof("infinite");

    cout << "DNF" << endl;
    vector<shared_ptr<Relation>> c = {r1, r2};
    auto dnf = RegularTableau::DNF(c);
    for (auto clause : dnf)
    {
        cout << " |" << clause.size();
        for (auto literal : clause)
        {
            cout << literal->toString() << " , ";
        }
    }

    shared_ptr<RegularTableau::Node> node = make_shared<RegularTableau::Node>(dnf[0]);
    RegularTableau regularTableau{node};
    regularTableau.solve();

    ofstream file2("reg.dot");
    regularTableau.toDotFormat(file2);

    /*vector<string> allArgs(argv, argv + argc);

    // setup solvers
    Solver solver;
    loadTheory(solver);
    solver.logLevel = 1;
    Inequality uniproc = make_shared<ProofNode>("(po-loc | co | fr | rf)^+ & id", "0");
    Solver uniprocSolver;
    loadTheory(uniprocSolver);
    uniprocSolver.theory.insert(uniproc);
    uniprocSolver.logLevel = 1;

    // solver.solve("cat/sc.cat", "cat/oota.cat");
    // solver.exportProof("SC<=OOTA");
    // solver.solve("cat/sc.cat", "cat/tso.cat");
    // solver.exportProof("SC<=TSO");
    // uniprocSolver.solve(make_shared<ProofNode>("rf & int", "po-loc"));
    // uniprocSolver.exportProof("rfi<=po-loc+Uniproc");
    // solver.stepwise = true;
    // solver.solve(make_shared<ProofNode>("rf;rf", "0"));
    // solver.exportProof("rfrf<=0");
    // solver.solve(make_shared<ProofNode>("a;(b|c)", "(a;b) | (a;c)"));
    // solver.exportProof("proof");

    // solver.solve(make_shared<ProofNode>("a;b", "(a;(b & int)) | (a;(b & ext))"));
    // solver.exportProof("proof");
    // uniprocSolver.solve(make_shared<ProofNode>("data;rf", "R*M | W*W"));
    // uniprocSolver.exportProof("proof");

    /* // guided (easy)
    uniprocSolver.solve(make_shared<ProofNode>("rf & int", "po"));
    // TODO: uniprocSolver.learnGoal(make_shared<ProofNode>("rf & int", "po")); // rf & int != rf, int
    // old guided: Inequality g = make_shared<ProofNode>("(data;rf)^+ & id", "((po & (R*M | W*W)) | rfe)^+");
    uniprocSolver.solve(make_shared<ProofNode>("(data;rf)^+ & id", "((po & (R*M | W*W)) | rfe)^+ & id"));
    uniprocSolver.exportProof("proof");
    // */

    /* apply simplify rule
    Inequality g = make_shared<ProofNode>("(data | addr | ctrl | rf)^+", "0");
    uniprocSolver.proved.insert(make_shared<ProofNode>("rf;rf", "0"));
    Solver::root = g;
    uniprocSolver.goals.push(g);
    uniprocSolver.simplifyTcRule(g);
    cout << (*(g->rightNode->left.begin()))->toString() << endl;
    uniprocSolver.exportProof("proof"); //*/

    /*// after simplify
    uniprocSolver.solve(make_shared<ProofNode>("rf & int", "po")); // guided
    // uniprocSolver.solve(make_shared<ProofNode>("(data | data;rf)^+ & id", "((po & (R*M | W*W)) | rfe)^+ & id"));
    // old symmetric: uniprocSolver.solve(make_shared<ProofNode>("(data | data;rf;data)^+ & id", "((po & (R*M | W*W)) | rfe)^+ & id"));
    // old sym: uniprocSolver.solve(make_shared<ProofNode>("((rf | id);(data | data;rf;data)^+;(rf | id)) & id", "((po & (R*M | W*W)) | rfe)^+ & id"));
    uniprocSolver.exportProof("proof"); //*/

    /* // after real simplify
    uniprocSolver.solve(make_shared<ProofNode>("rf & int", "po")); // guided
    uniprocSolver.solve(make_shared<ProofNode>("(data | id);(data | data;rf)^+ & id", "((po & (R*M | W*W)) | rfe)^+ & id"));
    uniprocSolver.exportProof("proof"); //*/

    // with simplify
    /*uniprocSolver.solve(make_shared<ProofNode>("rf & int", "po")); // guided
    uniprocSolver.solve(make_shared<ProofNode>("(data | rf)^+ & id", "((po & (R*M | W*W)) | rfe)^+ & id"));
    uniprocSolver.exportProof("proof"); //*/

    // solver.solve("cat/tso-modified.cat", "cat/oota.cat");
    // solver.exportProof("TSO<=OOTA");

    // KATER goals // TODO
    /*uniprocSolver.solve(make_shared<ProofNode>("(rf | co | fr)^+", "rf | ((co | fr);(rf | id))"));
    uniprocSolver.exportProof("proof");

    cout << Solver::iterations << " iterations" << endl; //*/
}