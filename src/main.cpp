#include <fstream>
#include <iostream>
#include <optional>

#include "Assumption.h"
#include "CatInferVisitor.h"
#include "LogicVisitor.h"
#include "RegularTableau.h"
#include "Relation.h"
#include "Tableau.h"

// helper
Relation loadModel(const std::string &file) {
  CatInferVisitor visitor;
  auto sc = visitor.parseMemoryModel("../cat/" + file);
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
  unionR->label = 0;
  return *unionR;
}

int main(int argc, const char *argv[]) {
  std::string programName = argv[0];
  std::vector<std::string> programArguments;
  if (argc > 1) {
    programArguments.assign(argv + 1, argv + argc);
  } else {
    std::cout << "> " << std::flush;
    std::string input;
    std::getline(std::cin, input);
    std::stringstream inputStream(input);
    std::string argument;
    while (inputStream >> argument) {
      programArguments.push_back(argument);
    }
  }

  if (programArguments.size() > 1 && programArguments[1] == "infinite") {
    const auto &[assumptions, assertion] = Logic::parse("../tests/" + programArguments[0]);
    Tableau tableau{std::get<0>(assertion), std::get<1>(assertion)};
    tableau.solve(200);
    tableau.exportProof("infinite");
  } else if (programArguments.size() > 1) {
    // memory models
    // TODO: load base theory
    auto lhsModel = loadModel(programArguments[0] + ".cat");
    auto rhsModel = loadModel(programArguments[1] + ".cat");
    rhsModel.negated = true;
    RegularTableau tableau{std::move(lhsModel), std::move(rhsModel)};
    tableau.solve();
    tableau.exportProof("regular");
    //*/
  } else {
    const auto &[assumptions, assertion] = Logic::parse("../tests/" + programArguments[0]);
    RegularTableau::assumptions = assumptions;
    RegularTableau tableau{std::get<0>(assertion), std::get<1>(assertion)};
    tableau.solve();
    tableau.exportProof("regular");
  }
}
