#pragma once
#include <optional>
#include <string>

#include "../Relation.h"

enum class ConstraintType { empty, irreflexive, acyclic };

class Constraint {
 public:
  Constraint(ConstraintType type, CanonicalRelation relation,
             const std::optional<std::string>& name = std::nullopt);

  ConstraintType type;
  CanonicalRelation relation;
  std::optional<std::string> name;  // for printing

  // TODO: void toEmptyNormalForm();
};
