#include <iostream>

#include "RegularTableau.h"
#include "Tableau.h"
#include "parsing/LogicVisitor.h"

int main(int argc, const char* argv[]) {
  Formula f("{e};{e}");
  Tableau t{std::move(f)};
  t.solve();
  t.exportProof("test");
  /*Set s1("{e}.(a & b)");
  std::cout << s1.toString() << std::endl;
  auto s2 = *s1.applyRule();
  for (const std::vector<Set::PartialPredicate>& clause : s2) {
    for (const Set::PartialPredicate& partialPredicate : clause) {
      const p = std::get_if<Predicate>(partialPredicate);
      if (p) {
        std::cout << p->toString() << std::endl;
      } else {
        auto s = std::get<Set>(partialPredicate);
        std::cout << s.toString() << std::endl;
      }
    }
  }*/

  // parse arguments or ask for arguments
  /*std::string programName = argv[0];
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

  std::string path = programArguments[0];
  const auto &[assumptions, goals] = Logic::parse(path);
  std::cout << "[Status] Parsing done: " << goals.size() << " goals, " << assumptions.size()
            << " assumptions" << std::endl;
  for (auto goal : goals) {
    std::cout << "[Status] Prove goal:" << goal.toString() << std::endl;
    if (programArguments.size() > 1 && programArguments[1] == "infinite") {
      Tableau tableau{goal};
      tableau.solve(200);
      tableau.exportProof("infinite");
    } else {
      RegularTableau::assumptions = assumptions;
      RegularTableau tableau{goal};
      tableau.solve();
      tableau.exportProof("regular");
    }
  }*/
}
