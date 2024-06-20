#include "Relation.h"

#include <algorithm>

std::unordered_map<Relation, const Relation> Relation::canonicalRelations;

Relation::Relation(const RelationOperation operation, CanonicalRelation left,
                   CanonicalRelation right, std::optional<std::string> identifier)
    : operation(operation), leftOperand(left), rightOperand(right), identifier(identifier){};

Relation::Relation(const Relation &&other)
    : operation(std::move(other.operation)),
      leftOperand(std::move(other.leftOperand)),
      rightOperand(std::move(other.rightOperand)),
      identifier(std::move(other.identifier)) {}

CanonicalRelation Relation::newRelation(const RelationOperation operation) {
  return newRelation(operation, nullptr, nullptr, std::nullopt);
}
CanonicalRelation Relation::newRelation(const RelationOperation operation, CanonicalRelation left) {
  return newRelation(operation, left, nullptr, std::nullopt);
}
CanonicalRelation Relation::newRelation(const RelationOperation operation, CanonicalRelation left,
                                        CanonicalRelation right,

                                        std::optional<std::string> identifier) {
  Relation r(operation, left, right, identifier);
  auto [canonicalRelationIterator, found] =
      Relation::canonicalRelations.try_emplace(std::move(r), operation, left, right, identifier);
  return &(*canonicalRelationIterator).second;
}
CanonicalRelation Relation::newRelation(const RelationOperation operation, CanonicalRelation left,
                                        CanonicalRelation right) {
  return newRelation(operation, left, right, std::nullopt);
}
CanonicalRelation Relation::newBaseRelation(const std::string &identifier) {
  return newRelation(RelationOperation::base, nullptr, nullptr, identifier);
}

bool Relation::operator==(const Relation &other) const {
  return operation == other.operation && leftOperand == other.leftOperand &&
         rightOperand == other.rightOperand && identifier == other.identifier;
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
    case RelationOperation::cartesianProduct:
      output += "TODO";  // "(" + TODO: leftRelation + "x" + TODO: rightRelation + ")";
  }
  return output;
}
