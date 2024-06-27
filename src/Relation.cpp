#include "Relation.h"

#include <cassert>
#include <unordered_set>

Relation::Relation(const RelationOperation operation, const CanonicalRelation left,
                   const CanonicalRelation right, std::optional<std::string> identifier)
    : operation(operation),
      identifier(std::move(identifier)),
      leftOperand(left),
      rightOperand(right) {}

CanonicalRelation Relation::newRelation(const RelationOperation operation) {
  return newRelation(operation, nullptr, nullptr, std::nullopt);
}
CanonicalRelation Relation::newRelation(const RelationOperation operation, const CanonicalRelation left) {
  return newRelation(operation, left, nullptr, std::nullopt);
}
CanonicalRelation Relation::newRelation(const RelationOperation operation,
                                        const CanonicalRelation left,
                                        const CanonicalRelation right,
                                        const std::optional<std::string> &identifier) {
#if (DEBUG)
  // ------------------ Validation ------------------
  static std::unordered_set operations = {
      RelationOperation::identity,     RelationOperation::cartesianProduct,
      RelationOperation::intersection, RelationOperation::composition,
      RelationOperation::converse,     RelationOperation::transitiveClosure,
      RelationOperation::choice,       RelationOperation::base,
      RelationOperation::full,         RelationOperation::empty};
  assert(operations.contains(operation));

  const bool isBinary = (left != nullptr && right != nullptr);
  const bool isUnary = (left == nullptr) == (right != nullptr);
  const bool isNullary = (left == nullptr && right == nullptr);
  const bool hasId = identifier.has_value();
  switch (operation) {
    case RelationOperation::base:
      assert(hasId && isNullary);
      break;
    case RelationOperation::identity:
    case RelationOperation::empty:
    case RelationOperation::full:
      assert(!hasId && isNullary);
      break;
    case RelationOperation::choice:
    case RelationOperation::intersection:
    case RelationOperation::composition:
      assert(!hasId && isBinary);
      break;
    case RelationOperation::transitiveClosure:
    case RelationOperation::converse:
      assert(!hasId && isUnary);
      break;
    case RelationOperation::cartesianProduct:
      // TODO: Cartesian product of relations???
      break;
  }
#endif

  static std::unordered_set<Relation> canonicalizer;
  auto [iter, created] = canonicalizer.emplace(operation, left, right, identifier);
  return &(*iter);
}
CanonicalRelation Relation::newRelation(const RelationOperation operation, const CanonicalRelation left,
                                        const CanonicalRelation right) {
  return newRelation(operation, left, right, std::nullopt);
}
CanonicalRelation Relation::newBaseRelation(std::string identifier) {
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
