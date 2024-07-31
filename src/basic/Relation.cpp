#include "Relation.h"

#include <cassert>
#include <unordered_set>

#include "../Stats.h"
#include "Set.h"

Relation::Relation(const RelationOperation operation, const CanonicalRelation left,
                   const CanonicalRelation right, std::optional<std::string> identifier,
                   const CanonicalSet set)
    : operation(operation),
      identifier(std::move(identifier)),
      leftOperand(left),
      rightOperand(right),
      set(set) {}

CanonicalRelation Relation::newRelation(const RelationOperation operation,
                                        const CanonicalRelation left, const CanonicalRelation right,
                                        const std::optional<std::string> &identifier,
                                        const CanonicalSet set) {
#if (DEBUG)
  // ------------------ Validation ------------------
  const bool isBinary = (left != nullptr && right != nullptr);
  const bool isUnary = (left == nullptr) == (right != nullptr);
  const bool isNullary = (left == nullptr && right == nullptr);
  const bool hasId = identifier.has_value();
  const bool hasSet = set != nullptr;
  switch (operation) {
    case RelationOperation::baseRelation:
      assert(hasId && isNullary && !hasSet);
      break;
    case RelationOperation::idRelation:
    case RelationOperation::emptyRelation:
    case RelationOperation::fullRelation:
      assert(!hasId && isNullary && !hasSet);
      break;
    case RelationOperation::relationUnion:
    case RelationOperation::relationIntersection:
    case RelationOperation::composition:
      assert(!hasId && isBinary && !hasSet);
      break;
    case RelationOperation::transitiveClosure:
    case RelationOperation::converse:
      assert(!hasId && isUnary && !hasSet);
      break;
    case RelationOperation::setIdentity:
      assert(!hasId && isNullary && hasSet);
      break;
    case RelationOperation::cartesianProduct:
      // TODO: Cartesian product of relations???
      throw std::logic_error("not implemented");
      break;
    default:
      assert(false);
      throw std::logic_error("unreachable");
  }
#endif

  static std::unordered_set<Relation> canonicalizer;
  auto [iter, created] = canonicalizer.emplace(operation, left, right, identifier, set);
  Stats::boolean("#relations").count(created);
  return &(*iter);
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
    case RelationOperation::setIdentity:
      output += "[" + set->toString() + "]";
      break;
    case RelationOperation::cartesianProduct:
      output += "TODO";  // "(" + TODO: leftRelation + "x" + TODO: rightRelation + ")";
    default:
      throw std::logic_error("unreachable");
  }
  return output;
}
