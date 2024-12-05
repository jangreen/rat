#pragma once
#include "helper/utility.h"
#include "parsing/Assumption.h"
#include "parsing/LogicVisitor.h"
#include "parsing/Preprocessing.h"
#include "regularTableau/RegularTableau.h"
#include "statistics/Stats.h"

inline std::chrono::steady_clock::time_point start;

inline std::vector<std::optional<bool>> rat(const std::string &path, int timeout = 0,
                                            bool quiet = false) {
  if (quiet) {
    std::cout.setstate(std::ios_base::failbit);
    spdlog::set_level(spdlog::level::off);
  }

  auto goals = Logic::parse(path);
  assert(validateDNF(goals));
  spdlog::info(fmt::format(
      "[Parser] Done: {} goal(s), {} relation assumption(s), {} set assumption(s)", goals.size(),
      Assumption::baseAssumptions.size() + Assumption::idAssumptions.size() +
          Assumption::emptinessAssumptions.size(),
      Assumption::setEmptinessAssumptions.size() + Assumption::baseSetAssumptions.size()));
  std::vector<std::optional<bool>> answers;
  for (auto &goal : goals) {
    // TODO: fix: preprocessing(goal);
    spdlog::info("[Status] Goal: ");
    print(goal);

    start = std::chrono::steady_clock::now();
    RegularTableau tableau(goal);
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