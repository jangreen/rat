#include <iostream>
#include "Relation.h"
#include "Tableau.h"
#include "RegularTableau.h"
#include "Assumption.h"

int main(int argc, const char *argv[])
{
    /* 1) *
    Relation r1("(id;a)^*");
    Relation r2("a;(a;a)^* | (a;a)^*");
    //*/
    /* 2) *
    Relation r1("a;a;a^*");
    Relation r2("a");
    Relation leftSide("a;a^*");
    Assumption transitiveA(AssumptionType::regular, std::move(leftSide), "a");
    RegularTableau::assumptions.push_back(std::move(transitiveA));
    //*/
    /* 3) *
    Relation r1("a;a^*");
    Relation r2("a");
    Relation leftSide("a;a");
    Assumption emptyAA(AssumptionType::empty, std::move(leftSide));
    RegularTableau::assumptions.push_back(std::move(emptyAA));
    //*/
    /* 4) *
    Relation r1("a;b;c");
    Relation r2("c");
    Relation leftSide("a;b");
    Assumption idAB(AssumptionType::identity, std::move(leftSide));
    RegularTableau::assumptions.push_back(std::move(idAB));
    //*/
    /* 5) KATER ECO PAPER */
    Relation r1("(rf | co | rfinv;co);(rf | co | rfinv;co)^*");
    Relation r2("rf | (co | rfinv;co);(rf | id)");
    Assumption coTransitive(AssumptionType::regular, Relation("co;co^*"), "co");
    RegularTableau::assumptions.push_back(std::move(coTransitive));
    Assumption rfrf(AssumptionType::empty, Relation("rf;rf"));
    Assumption rfco(AssumptionType::empty, Relation("rf;co"));
    Assumption corfinv(AssumptionType::empty, Relation("co;rfinv"));
    RegularTableau::assumptions.push_back(std::move(rfrf));
    RegularTableau::assumptions.push_back(std::move(rfco));
    RegularTableau::assumptions.push_back(std::move(corfinv));
    Assumption rfrfinv(AssumptionType::identity, Relation("rf;rfinv"));
    RegularTableau::assumptions.push_back(std::move(rfrfinv));
    /* 6) REGINCL *
    Relation r1("(a;a|b)^*;a;b");
    Relation r2("(a;(a;a)^* | b)^*;b");
    //*/

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
    std::cout << "|=" << r1.toString() << " & " << r2.toString() << std::endl;
    //*/

    /* INFINITE
    cout << "Infinite Proof..." << endl;
    Tableau tableau{r1, r2};
    tableau.solve(200);
    tableau.exportProof("infinite");
    //*/

    /* REGULAR */
    std::cout << "Regular Proof..." << std::endl;
    RegularTableau regularTableau{r1, r2};
    std::cout << "Done. " << regularTableau.solve() << std::endl;
    std::cout << "Export Regular Tableau..." << std::endl;
    regularTableau.exportProof("regular");
    //*/
}