#include <iostream>

#include "RegularTableau.h"
#include "Tableau.h"
#include "parsing/LogicVisitor.h"

int main(int argc, const char *argv[]) {
  // parse arguments or ask for arguments
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

  std::string path = programArguments[0];
  const auto &[assumptions, assertions] = Logic::parse(path);
  std::cout << "[Status] Parsing done: " << assertions.size() << " goals" << std::endl;
  for (auto assertion : assertions) {
    std::cout << "[Status] Prove goal:" << std::get<0>(assertion).toString() << " /\\ "
              << std::get<1>(assertion).toString() << std::endl;
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
  }
}
