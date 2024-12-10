#include "Rat.h"

#include "preprocessing/Preprocessing.h"
#include "regularTableau/RegularTableau.h"
#include "statistics/Stats.h"

std::vector<std::optional<bool>> RatSolver::rat(const std::string &path, int timeout, bool quiet) {
  if (quiet) {
    std::cout.setstate(std::ios_base::failbit);
    spdlog::set_level(spdlog::level::off);
  }

  auto goals = parser.parse(path);
  assert(validateDNF(; goals));
  spdlog::info(fmt::format(
      "[Parser] Done: {} goal(s), {} relation assumption(s), {} set assumption(s)", goals.size(),
      parser.getAssumptions().baseAssumptions.size() +
          parser.getAssumptions().idAssumptions.size() +
          parser.getAssumptions().emptinessAssumptions.size(),
      parser.getAssumptions().setEmptinessAssumptions.size() +
          parser.getAssumptions().baseSetAssumptions.size()));
  std::vector<std::optional<bool>> answers;
  for (auto &goal : goals) {
    preprocessing(goal);
    spdlog::info("[Status] Goal: ");
    print(goal);

    start = std::chrono::steady_clock::now();
    RegularTableau tableau(goal, parser.getAssumptions());
    const auto answer = tableau.solve(timeout);
    answers.push_back(answer);

    Stats::print();
    Stats::reset();

    if (answer) {
      spdlog::info(fmt::format("[Solver] Duration: {} seconds", since(start)));
      spdlog::info("[Solver] Answer: " + std::to_string(answer.value()));
    } else {
      spdlog::info("[Solver] Timeout");
    }
  }

  if (quiet) {
    std::cout.clear();
  }

  return answers;
}

const Logic &RatSolver::getParser() { return parser; }

void RatSolver::preprocessing(Cube &goal) {
  Preprocessing::eleminateRedundantConjunctiveContexts(goal, parser.getAssumptions());
  Preprocessing::replaceEmptyExpressionsInNegatedLiterals(goal, parser.getAssumptions());
  spdlog::info("[Status] Preprocesing done.");
}