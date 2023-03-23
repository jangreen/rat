#include <fstream>
#include <iostream>
#include <optional>

#include "Assumption.h"
#include "CatInferVisitor.h"
#include "LogicVisitor.h"
#include "RegularTableau.h"
#include "Relation.h"
#include "Tableau.h"

int main(int argc, const char *argv[]) {
  std::string programName = argv[0];
  std::vector<std::string> programArguments;
  if (argc > 1) {
    programArguments.assign(argv + 1, argv + argc);
  } else {
    // TODO: gui, ask for test
    // return 0;
    programArguments.push_back("test0");
  }

  const auto &[assumptions, assertion] = Logic::parse("../tests/" + programArguments[0]);
  if (programArguments.size() > 1 && programArguments[1] == "infinite") {
    Tableau tableau{std::get<0>(assertion), std::get<1>(assertion)};
    tableau.solve(200);
    tableau.exportProof("infinite");
  } else {
    RegularTableau::assumptions = assumptions;
    RegularTableau tableau{std::get<0>(assertion), std::get<1>(assertion)};
    tableau.solve();
    tableau.exportProof("regular");
  }

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

  RegularTableau regularTableau(initialClause);
  regularTableau.solve();
  regularTableau.exportProof("regular");
  //*/
}
