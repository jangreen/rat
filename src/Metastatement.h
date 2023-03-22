#pragma once
#include <optional>
#include <string>

enum class MetastatementType { labelEquality, labelRelation };

class Metastatement {
 public:
  Metastatement(const MetastatementType type, int label1, int label2,
                std::optional<std::string> baseRelation = std::nullopt);

  MetastatementType type;
  int label1;
  int label2;
  std::optional<std::string> baseRelation;  // is set iff labelRelation

  std::string toString() const;  // for printing
};
