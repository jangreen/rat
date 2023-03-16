#include <memory>
#include <iostream>
#include <fstream>
#include <vector>
#include "Relation.h"
#include "Tableau.h"
#include "RegularTableau.h"
#include "Assumption.h"

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
    /* 1) */
    Relation r1 = Relation::parse("(id;a)^*");
    Relation r2 = Relation::parse("a;(a;a)^* | (a;a)^*");
    //*/
    /* 2)
    Relation r1 = Relation::parse("a;a;a^*");
    Relation r2 = Relation::parse("a");
    Relation leftSide = Relation::parse("a;a^*");
    Assumption transitiveA = Assumption(AssumptionType::regular, std::move(leftSide), "a");
    RegularTableau::assumptions.push_back(std::move(transitiveA));
    //*/
    /* 3)
    Relation r1 = Relation::parse("a;a^*");
    Relation r2 = Relation::parse("a");
    Relation leftSide = Relation::parse("a;a");
    Assumption emptyAA = Assumption(AssumptionType::empty, std::move(leftSide));
    RegularTableau::assumptions.push_back(std::move(emptyAA));
    //*/
    /* 4)
    Relation r1 = Relation::parse("a;b;c");
    Relation r2 = Relation::parse("c");
    Relation leftSide = Relation::parse("a;b");
    Assumption idAB = Assumption(AssumptionType::identity, std::move(leftSide));
    RegularTableau::assumptions.push_back(std::move(idAB));
    //*/
    /* 5) KATER ECO PAPER
    Relation r1 = Relation::parse("(rf | co | rfinv;co);(rf | co | rfinv;co)^*");
    Relation r2 = Relation::parse("rf | (co | rfinv;co);(rf | id)");
    Assumption coTransitive = Assumption(AssumptionType::regular, Relation::parse("co;co^*"), "co");
    RegularTableau::assumptions.push_back(std::move(coTransitive));
    Assumption rfrf = Assumption(AssumptionType::empty, Relation::parse("rf;rf"));
    Assumption rfco = Assumption(AssumptionType::empty, Relation::parse("rf;co"));
    Assumption corfinv = Assumption(AssumptionType::empty, Relation::parse("co;rfinv"));
    RegularTableau::assumptions.push_back(std::move(rfrf));
    RegularTableau::assumptions.push_back(std::move(rfco));
    RegularTableau::assumptions.push_back(std::move(corfinv));
    Assumption rfrfinv = Assumption(AssumptionType::identity, Relation::parse("rf;rfinv"));

    /*
    assume co;co <= co
    assume co;rf-1 = 0
    assume rf;rf = 0
    assume rf;co = 0
    assume rf;rf-1 <= id
    */
    //*/

    /* PROOF SETUP */
    r1.label = 0;
    r1.negated = false;
    r2.label = 0;
    r2.negated = true;
    std::cout << "|=" << r1.toString() << " & " << r2.toString() << endl;
    //*/

    /* INFINITE */
    cout << "Infinite Proof..." << endl;
    Tableau tableau{r1, r2};
    tableau.solve(100);
    tableau.exportProof("infinite");
    //*/

    /* REGULAR */
    std::cout << "Regular Proof..." << endl;
    RegularTableau regularTableau{r1, r2};
    regularTableau.solve();
    std::cout << "Export Regular Tableau..." << endl;
    regularTableau.exportProof("reg");
    //*/
}