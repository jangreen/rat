#include <iostream>
#include <optional>

#include "Assumption.h"
#include "CatInferVisitor.h"
#include "RegularTableau.h"
#include "Relation.h"
#include "Tableau.h"

int main(int argc, const char *argv[]) {
  /* 1) *
  Relation r1("(id;a)^*");
  Relation r2("a;(a;a)^* | (a;a)^*");
  //*/
  /* 2) KATER ECO PAPER *
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
  //*/
  /* Intersections */
  Relation r1("a;(b & c)");
  Relation r2("a;c & a;b");
  //*/
  /* Intersections Counterexample *
  Relation r2("a;(b & c)");
  Relation r1("a;c & a;b");
  //*/

  /* PROOF SETUP */
  r1.label = 0;
  r1.negated = false;
  r2.label = 0;
  r2.negated = true;
  std::cout << "|=" << r1.toString() << " & " << r2.toString() << std::endl;
  //*/

  /* INFINITE *
  std::cout << "Infinite Proof..." << std::endl;
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

  /*---------------  Memory models  --------------------*

  Clause initialClause;
  CatInferVisitor visitor;
  auto sc = visitor.parseMemoryModel("../cat/sc.cat");
  std::optional<Relation> unionR;
  for (auto &constraint : sc) {
    constraint.toEmptyNormalForm();
    Relation r = constraint.relation;
    if (unionR) {
      unionR = Relation(Operation::choice, std::move(*unionR), std::move(r));
    } else {
      unionR = r;
    }
  }
  Relation r = *unionR;
  r.label = 0;
  // r.negated = true;
  initialClause.push_back(std::move(r));

  auto tso = visitor.parseMemoryModel("../cat/tso.cat");
  std::optional<Relation> unionR2;
  for (auto &constraint : tso) {
    constraint.toEmptyNormalForm();
    Relation r = constraint.relation;
    if (unionR2) {
      unionR2 = Relation(Operation::choice, std::move(*unionR2), std::move(r));
    } else {
      unionR2 = r;
    }
  }
  Relation r2 = *unionR2;
  r2.label = 0;
  r2.negated = true;
  initialClause.push_back(std::move(r2));

  std::cout << "Regular Proof..." << std::endl;
  RegularTableau regularTableau(initialClause);
  std::cout << "Done. " << regularTableau.solve() << std::endl;
  regularTableau.exportProof("regular");
  //*/
}
