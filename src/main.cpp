#include <spdlog/spdlog.h>

#include <iostream>

#include "Assumption.h"
#include "Preprocessing.h"
#include "Stats.h"
#include "parsing/LogicVisitor.h"
#include "regularTableau/RegularTableau.h"
#include "utility.h"

// helper
template <class result_t = std::chrono::milliseconds, class clock_t = std::chrono::steady_clock,
          class duration_t = std::chrono::milliseconds>
auto since(std::chrono::time_point<clock_t, duration_t> const &start) {
  constexpr double millisecondInSeconds = 1000;
  return std::chrono::duration_cast<result_t>(clock_t::now() - start).count() /
         millisecondInSeconds;
}

int main(int argc, const char *argv[]) {
#if DEBUG
  spdlog::set_level(spdlog::level::debug);  // Set global log level to debug
#endif
  // parse arguments or ask for arguments argv[0] is path to executed program
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
  auto goals = Logic::parse(path);
  assert(validateDNF(goals));
  spdlog::info(fmt::format(
      "[Parser] Done: {} goal(s), {} relation assumption(s), {} set assumption(s)", goals.size(),
      Assumption::baseAssumptions.size() + Assumption::idAssumptions.size() +
          Assumption::emptinessAssumptions.size(),
      Assumption::setEmptinessAssumptions.size() + Assumption::baseSetAssumptions.size()));
  for (auto &goal : goals) {
    preprocessing(goal);
    spdlog::info("[Status] Goal: ");
    print(goal);

    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    RegularTableau tableau(goal);
    tableau.solve();
    spdlog::info(fmt::format("[Solver] Duration: {} seconds", since(start)));

    Stats::print();
    Stats::reset();
  }
}
