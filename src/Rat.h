#pragma once
#include "helper/utility.h"
#include "parsing/LogicVisitor.h"

inline std::chrono::steady_clock::time_point start;

class RatSolver {
  Logic parser;

 public:
  std::vector<std::optional<bool>> rat(const std::string& path, int timeout = 0,
                                       bool quiet = false);

  void preprocessing(Cube& goal);

  const Logic& getParser();
};
