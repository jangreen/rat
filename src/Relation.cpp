#include "Relation.h"

#include <cassert>
#include <unordered_set>

Relation::Relation(const RelationOperation operation, const CanonicalRelation left,
                   const CanonicalRelation right, std::optional<std::string> identifier)
    : operation(operation),
      identifier(std::move(identifier)),
      leftOperand(left),
      rightOperand(right) {}

CanonicalRelation Relation::newRelation(const RelationOperation operation,
                                        const CanonicalRelation left) {
  return newRelation(operation, left, nullptr, std::nullopt);
}
CanonicalRelation Relation::newRelation(const RelationOperation operation,
                                        const CanonicalRelation left, const CanonicalRelation right,
                                        const std::optional<std::string> &identifier) {
#if (DEBUG)
  // ------------------ Validation ------------------
  static std::unordered_set operations = {RelationOperation::idRelation,
                                          RelationOperation::cartesianProduct,
                                          RelationOperation::relationIntersection,
                                          RelationOperation::composition,
                                          RelationOperation::converse,
                                          RelationOperation::transitiveClosure,
                                          RelationOperation::relationUnion,
                                          RelationOperation::baseRelation,
                                          RelationOperation::fullRelation,
                                          RelationOperation::emptyRelation};
  assert(operations.contains(operation));

  const bool isBinary = (left != nullptr && right != nullptr);
  const bool isUnary = (left == nullptr) == (right != nullptr);
  const bool isNullary = (left == nullptr && right == nullptr);
  const bool hasId = identifier.has_value();
  switch (operation) {
    case RelationOperation::baseRelation:
      assert(hasId && isNullary);
      break;
    case RelationOperation::idRelation:
    case RelationOperation::emptyRelation:
    case RelationOperation::fullRelation:
      assert(!hasId && isNullary);
      break;
    case RelationOperation::relationUnion:
    case RelationOperation::relationIntersection:
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
    default:
      throw std::logic_error("unreachable");
  }
#endif

  static std::unordered_set<Relation> canonicalizer;
  auto [iter, created] = canonicalizer.emplace(operation, left, right, identifier);
  return &(*iter);
}
CanonicalRelation Relation::newRelation(const RelationOperation operation,
                                        const CanonicalRelation left,
                                        const CanonicalRelation right) {
  return newRelation(operation, left, right, std::nullopt);
}

bool Relation::operator==(const Relation &other) const {
  return operation == other.operation && leftOperand == other.leftOperand &&
         rightOperand == other.rightOperand && identifier == other.identifier;
}

std::string Relation::toString() const {
  std::string output;
  switch (operation) {
    case RelationOperation::relationIntersection:
      output += "(" + leftOperand->toString() + " & " + rightOperand->toString() + ")";
      break;
    case RelationOperation::composition:
      output += "(" + leftOperand->toString() + ";" + rightOperand->toString() + ")";
      break;
    case RelationOperation::relationUnion:
      output += "(" + leftOperand->toString() + " | " + rightOperand->toString() + ")";
      break;
    case RelationOperation::converse:
      output += leftOperand->toString() + "^-1";
      break;
    case RelationOperation::transitiveClosure:
      output += leftOperand->toString() + "^*";
      break;
    case RelationOperation::baseRelation:
      output += *identifier;
      break;
    case RelationOperation::idRelation:
      output += "id";
      break;
    case RelationOperation::emptyRelation:
      output += "0";
      break;
    case RelationOperation::fullRelation:
      output += "1";
      break;
    case RelationOperation::cartesianProduct:
      output += "TODO";  // "(" + TODO: leftRelation + "x" + TODO: rightRelation + ")";
    default:
      throw std::logic_error("unreachable");
  }
  return output;
}
