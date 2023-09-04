#include "Relation.h"

#include <algorithm>

#include "parsing/CatInferVisitor.h"

Relation::Relation(const Relation &other)
    : operation(other.operation),
      identifier(other.identifier),
      saturated(other.saturated),
      saturatedId(other.saturatedId) {
  if (other.leftOperand != nullptr) {
    leftOperand = std::make_unique<Relation>(*other.leftOperand);
  }
  if (other.rightOperand != nullptr) {
    rightOperand = std::make_unique<Relation>(*other.rightOperand);
  }
}
Relation &Relation::operator=(const Relation &other) {
  Relation copy(other);
  swap(*this, copy);
  return *this;
}
Relation::Relation(const std::string &expression) {
  CatInferVisitor visitor;
  *this = visitor.parseRelation(expression);
}
Relation::Relation(const RelationOperation operation, const std::optional<std::string> &identifier)
    : operation(operation), identifier(identifier), leftOperand(nullptr), rightOperand(nullptr) {}
Relation::Relation(const RelationOperation operation, Relation &&left)
    : operation(operation), identifier(std::nullopt), rightOperand(nullptr) {
  leftOperand = std::make_unique<Relation>(std::move(left));
}
Relation::Relation(const RelationOperation operation, Relation &&left, Relation &&right)
    : operation(operation), identifier(std::nullopt) {
  leftOperand = std::make_unique<Relation>(std::move(left));
  rightOperand = std::make_unique<Relation>(std::move(right));
}

std::unordered_map<std::string, Relation> Relation::relations;
int Relation::maxLabel = 0;

bool Relation::operator==(const Relation &otherRelation) const {
  if (operation != otherRelation.operation) {
    return false;
  }

  /* modulo negation if (negated != otherRelation.negated)
  {
      return false;
  }*/

  if ((label.has_value() != otherRelation.label.has_value()) ||
      (label && *label != *otherRelation.label)) {
    return false;
  }

  if (operation == RelationOperation::base) {
    return *identifier == *otherRelation.identifier;
  }

  if (operation == RelationOperation::identity || operation == RelationOperation::empty ||
      operation == RelationOperation::full) {
    return true;
  }

  bool leftEqual = *leftOperand == *otherRelation.leftOperand;
  bool rightNullEqual = (rightOperand == nullptr) == (otherRelation.rightOperand == nullptr);
  bool rightEqual =
      rightNullEqual && (rightOperand == nullptr || *rightOperand == *otherRelation.rightOperand);
  return leftEqual && rightEqual;
}
bool Relation::operator<(const Relation &otherRelation) const {
  return (toString() < otherRelation.toString());
}

// helper
bool isNormalHelper(const Relation &relation, bool negated) {
  if (relation.label && relation.operation == RelationOperation::converse) {
    return true;  // assume that converse only occurs on base realtions
  }
  if (relation.label && relation.operation != RelationOperation::base) {
    return false;
  } else if (relation.leftOperand != nullptr) {  // has children
    bool leftIsNormal =
        relation.leftOperand == nullptr || isNormalHelper(*relation.leftOperand, negated);
    bool rightIsNormal =
        relation.rightOperand == nullptr || isNormalHelper(*relation.rightOperand, negated);
    bool leftRightSpecialCase =
        negated || relation.leftOperand == nullptr || relation.rightOperand == nullptr;
    return leftIsNormal && rightIsNormal && leftRightSpecialCase;
  }
  return true;
}

bool Relation::isNormal() const { return isNormalHelper(*this, negated); }

std::vector<int> Relation::labels() const {
  if (label) {
    return {*label};
  } else if (leftOperand == nullptr && rightOperand == nullptr) {
    return {};
  }

  auto result = leftOperand->labels();
  if (rightOperand != nullptr) {
    auto right = rightOperand->labels();
    // only unique
    for (int i : right) {
      if (std::find(result.begin(), result.end(), i) == result.end()) {
        result.push_back(i);
      }
    }
  }
  return result;
}

std::vector<int> Relation::calculateRenaming() const {
  return labels();  // labels already calculates the renaming
}

void Relation::rename(const std::vector<int> &renaming) {
  if (label) {
    label = distance(renaming.begin(), find(renaming.begin(), renaming.end(), *label));
  } else if (leftOperand) {
    leftOperand->rename(renaming);
    if (rightOperand) {
      rightOperand->rename(renaming);
    }
  }
}

void Relation::inverseRename(const std::vector<int> &renaming) {
  if (label) {
    label = renaming[*label];
  } else if (leftOperand) {
    leftOperand->inverseRename(renaming);
    if (rightOperand) {
      rightOperand->inverseRename(renaming);
    }
  }
}

std::string Relation::toString() const {
  std::string output;
  switch (operation) {
    case RelationOperation::intersection:
      output += "(" + leftOperand->toString() + " & " + rightOperand->toString() + ")";
      break;
    case RelationOperation::composition:
      output += "(" + leftOperand->toString() + ";" + rightOperand->toString() + ")";
      break;
    case RelationOperation::choice:
      output += "(" + leftOperand->toString() + " | " + rightOperand->toString() + ")";
      break;
    case RelationOperation::converse:
      output += leftOperand->toString() + "^-1";
      break;
    case RelationOperation::transitiveClosure:
      output += leftOperand->toString() + "^*";
      break;
    case RelationOperation::base:
      output += *identifier;
      break;
    case RelationOperation::identity:
      output += "id";
      break;
    case RelationOperation::empty:
      output += "0";
      break;
    case RelationOperation::full:
      output += "1";
      break;
  }
  if (saturated) {
    output += ".";
  }
  if (saturatedId) {
    output += "..";
  }
  return output;
}

template <>
Relation Relation::substituteLeft(Relation &&substitutedRelation) {
  // fix label
  if (operation == RelationOperation::composition &&
      substitutedRelation.operation == RelationOperation::none && substitutedRelation.label) {
    int l = *substitutedRelation.label;
    substitutedRelation = *rightOperand;
    substitutedRelation.label = l;
    return substitutedRelation;
  }
  Relation result(operation, std::move(substitutedRelation), Relation(*rightOperand));
  result.negated = negated;
  return result;
}
template <>
std::tuple<Relation, Relation> Relation::substituteLeft(std::tuple<Relation, Relation> &&newLeft) {
  auto &[newRelation1, newRelation2] = newLeft;
  std::tuple<Relation, Relation> result{substituteLeft(std::move(newRelation1)),
                                        substituteLeft(std::move(newRelation2))};
  return result;
}
template <>
std::tuple<Relation, Metastatement> Relation::substituteLeft(
    std::tuple<Relation, Metastatement> &&newLeft) {
  auto &[newRelation, newMetastatement] = newLeft;
  std::tuple<Relation, Metastatement> result{substituteLeft(std::move(newRelation)),
                                             std::move(newMetastatement)};
  return result;
}
template <>
Relation Relation::substituteRight(Relation &&newRight) {
  Relation result(operation, Relation(*leftOperand), std::move(newRight));
  result.negated = negated;
  return result;
}
template <>
std::tuple<Relation, Relation> Relation::substituteRight(
    std::tuple<Relation, Relation> &&newRight) {
  auto &[newRelation1, newRelation2] = newRight;
  std::tuple<Relation, Relation> result{substituteRight(std::move(newRelation1)),
                                        substituteRight(std::move(newRelation2))};
  return result;
}
template <>
std::tuple<Relation, Metastatement> Relation::substituteRight(
    std::tuple<Relation, Metastatement> &&newLeft) {
  auto &[newRelation, newMetastatement] = newLeft;
  std::tuple<Relation, Metastatement> result{substituteRight(std::move(newRelation)),
                                             std::move(newMetastatement)};
  return result;
}
template <>
std::optional<std::tuple<Relation, Relation>> Relation::applyRule<ProofRule::choice>(
    const Metastatement *metastatement) {
  if (label && operation == RelationOperation::choice) {
    // Rule::choice, Rule::negChoice
    Relation r1(*leftOperand);
    r1.label = label;
    r1.negated = negated;
    Relation r2(*rightOperand);
    r2.label = label;
    r2.negated = negated;
    std::tuple<Relation, Relation> result{std::move(r1), std::move(r2)};
    return result;
  }
  return std::nullopt;
}
template <>
std::optional<std::tuple<Relation, Relation>> Relation::applyRule<ProofRule::transitiveClosure>(
    const Metastatement *metastatement) {
  if (label && operation == RelationOperation::transitiveClosure) {
    // Rule::transitiveClosure, Rule::negTransitiveClosure
    Relation r1(*leftOperand);
    r1.label = label;
    Relation tcCopy(*this);
    tcCopy.label = std::nullopt;
    tcCopy.negated = false;
    Relation newTC(RelationOperation::composition, std::move(r1), std::move(tcCopy));
    newTC.negated = negated;
    Relation r2(RelationOperation::none);  // empty relation
    r2.label = label;
    r2.negated = negated;
    std::tuple<Relation, Relation> result{std::move(r2), std::move(newTC)};
    return result;
  }
  return std::nullopt;
}
template <>
std::optional<std::tuple<Relation, Metastatement>> Relation::applyRule<ProofRule::at>(
    const Metastatement *metastatement) {
  if (leftOperand != nullptr && rightOperand != nullptr &&
      leftOperand->operation == RelationOperation::none &&
      rightOperand->operation == RelationOperation::none &&
      operation == RelationOperation::intersection) {
    // identify labels
    Relation r1(RelationOperation::none);
    r1.label = std::min(*leftOperand->label, *rightOperand->label);
    Metastatement m(MetastatementType::labelEquality, *leftOperand->label, *rightOperand->label);
    std::tuple<Relation, Metastatement> result{std::move(r1), std::move(m)};
    return result;
  }
  return std::nullopt;
}
template <>
std::optional<Relation> Relation::applyRule<ProofRule::negAt>(const Metastatement *metastatement) {
  if (leftOperand != nullptr && rightOperand != nullptr &&
      leftOperand->operation == RelationOperation::none &&
      rightOperand->operation == RelationOperation::none &&
      operation == RelationOperation::intersection && *leftOperand->label == *rightOperand->label) {
    Relation r1(RelationOperation::none);
    r1.label = *leftOperand->label;
    return r1;
  }
  return std::nullopt;
}
template <>
std::optional<std::tuple<Relation, Metastatement>> Relation::applyRule<ProofRule::a>(
    const Metastatement *metastatement) {
  if (label && operation == RelationOperation::base) {
    // Rule::a
    Relation r1(RelationOperation::none);  // empty relation
    Relation::maxLabel++;
    r1.label = Relation::maxLabel;
    Metastatement m(MetastatementType::labelRelation, *label, Relation::maxLabel, *identifier);
    std::tuple<Relation, Metastatement> result{std::move(r1), std::move(m)};
    return result;
  } else if (label && operation == RelationOperation::converse) {
    // Rule::negA
    Relation r1(RelationOperation::none);  // empty relation
    Relation::maxLabel++;
    r1.label = Relation::maxLabel;
    Metastatement m(MetastatementType::labelRelation, Relation::maxLabel, *label,
                    *leftOperand->identifier);
    std::tuple<Relation, Metastatement> result{std::move(r1), std::move(m)};
    return result;
  }
  return std::nullopt;
}
template <>
std::optional<Relation> Relation::applyRule<ProofRule::intersection>(
    const Metastatement *metastatement) {
  if (label && operation == RelationOperation::intersection) {
    // Rule::inertsection, Rule::negIntersection
    Relation r1(*leftOperand);
    r1.label = label;
    Relation r2(*rightOperand);
    r2.label = label;
    Relation result(RelationOperation::intersection, std::move(r1), std::move(r2));
    result.negated = negated;
    return result;
  }
  return std::nullopt;
}
template <>
std::optional<Relation> Relation::applyRule<ProofRule::composition>(
    const Metastatement *metastatement) {
  if (label && operation == RelationOperation::composition) {
    // Rule::composition, Rule::negComposition
    Relation r1(*leftOperand);
    r1.label = label;
    Relation r2(*rightOperand);
    Relation result(RelationOperation::composition, std::move(r1), std::move(r2));
    result.negated = negated;
    return result;
  }
  return std::nullopt;
}
template <>
std::optional<Relation> Relation::applyRule<ProofRule::id>(const Metastatement *metastatement) {
  if (label && operation == RelationOperation::identity) {
    // Rule::id, Rule::negId
    Relation r1(RelationOperation::none);  // empty relation
    r1.label = label;
    r1.negated = negated;
    return r1;
  }
  return std::nullopt;
}
template <>
std::optional<Relation> Relation::applyRule<ProofRule::propagation>(
    const Metastatement *metastatement) {
  int maxLabel = std::max(metastatement->label1, metastatement->label2);
  int minLabel = std::min(metastatement->label1, metastatement->label2);

  if (label && *label == maxLabel) {
    // Rule::prop
    Relation r1(*this);
    r1.label = minLabel;
    return r1;
  }
  return std::nullopt;
}
template <>
std::optional<Relation> Relation::applyRule<ProofRule::negA>(const Metastatement *metastatement) {
  if (operation == RelationOperation::base &&
      metastatement->type == MetastatementType::labelRelation && metastatement->label1 == *label &&
      metastatement->baseRelation == *identifier) {
    // Rule::negA
    Relation r1(RelationOperation::none);
    r1.label = metastatement->label2;
    return r1;
  } else if (operation == RelationOperation::converse &&
             metastatement->type == MetastatementType::labelRelation &&
             metastatement->label2 == *label &&
             metastatement->baseRelation == *leftOperand->identifier) {
    // assumption: converse occurs only on base relations
    // Rule::negConverseA
    Relation r1(RelationOperation::none);
    r1.label = metastatement->label1;
    return r1;
  }
  return std::nullopt;
}
