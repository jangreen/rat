#include <spdlog/spdlog.h>

#include <chrono>
#include <format>
#include <fstream>
#include <iostream>

#include "RegularTableau.h"
#include "Tableau.h"
#include "parsing/LogicVisitor.h"

// helper
template <class result_t = std::chrono::milliseconds, class clock_t = std::chrono::steady_clock,
          class duration_t = std::chrono::milliseconds>
auto since(std::chrono::time_point<clock_t, duration_t> const &start) {
  return std::chrono::duration_cast<result_t>(clock_t::now() - start);
}

int main(int argc, const char *argv[]) {
#if DEBUG
  spdlog::set_level(spdlog::level::debug);  // Set global log level to debug
#endif
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
  // TODO: const auto& [assumptions, goals] = Logic::parse(path);
  const auto &goals = Logic::parse(path);
  spdlog::info(
      fmt::format("[Parser] Done: {} goal(s), {} assumption(s)", goals.size(),
                  (RegularTableau::baseAssumptions.size() + RegularTableau::idAssumptions.size() +
                   RegularTableau::emptinessAssumptions.size())));
  for (auto goal : goals) {
    spdlog::info(fmt::format("[Status] Prove goal: {}", goal.toString()));
    if (programArguments.size() > 1 && programArguments[1] == "infinite") {
      Tableau tableau{goal};
      tableau.solve(200);
      tableau.exportProof("infinite");
    } else {
      // TODO: RegularTableau::assumptions = assumptions;
      RegularTableau tableau{goal};

      std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
      tableau.solve();

      spdlog::info(fmt::format("[Solver] Duration: {} seconds", since(start).count() / 1000.0));
      tableau.exportProof("regular");
    }
  }
}
