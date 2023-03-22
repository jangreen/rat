#include "Metastatement.h"

Metastatement::Metastatement(const MetastatementType type, int label1, int label2,
                             std::optional<std::string> baseRelation)
    : type(type), label1(label1), label2(label2), baseRelation(baseRelation) {}

std::string Metastatement::toString() const {
  std::string output;
  if (type == MetastatementType::labelRelation) {
    output = "(" + std::to_string(label1) + "," + std::to_string(label2) + ") -> " + *baseRelation;
  } else {
    output = std::to_string(label1) + " = " + std::to_string(label2);
  }
  return output;
}