#include "Relation.h"

#include <algorithm>

#include "parsing/LogicVisitor.h"

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
  Logic visitor;
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

bool Relation::operator==(const Relation &other) const {
  auto isEqual = operation == other.operation;
  if ((leftOperand == nullptr) != (other.leftOperand == nullptr)) {
    isEqual = false;
  } else if (leftOperand != nullptr && *leftOperand != *other.leftOperand) {
    isEqual = false;
  } else if ((rightOperand == nullptr) != (other.rightOperand == nullptr)) {
    isEqual = false;
  } else if (rightOperand != nullptr && *rightOperand != *other.rightOperand) {
    isEqual = false;
  } else if (identifier.has_value() != other.identifier.has_value()) {
    isEqual = false;
  } else if (identifier.has_value() && *identifier != *other.identifier) {
    isEqual = false;
  }
  return isEqual;
}
// TODO: remove: bool Relation::operator<(const Relation &other) const { return toString() <
// other.toString(); }

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
    case RelationOperation::cartesianProduct:
      output += "TODO";  // "(" + TODO: leftSet + "x" + TODO: rightSet + ")";
  }
  /*if (saturated) {
    output += ".";
  }
  if (saturatedId) {
    output += "..";
  }*/
  return output;
}
