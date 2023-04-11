#pragma once
#include <optional>
#include <string>

#include "../Relation.h"

enum class ConstraintType { empty, irreflexive, acyclic };

class Constraint {
 public:
  Constraint(const ConstraintType type, const Relation &&relation,
             const std::optional<std::string> name = std::nullopt);

  ConstraintType type;
  Relation relation;
  std::optional<std::string> name;  // for printing

  void toEmptyNormalForm();
};
