#include "Relation.h"

#include <cassert>
#include <unordered_set>

#include "../statistics/Stats.h"
#include "Set.h"

Relation::Relation(const RelationOperation operation, const CanonicalRelation left,
                   const CanonicalRelation right, std::optional<std::string> identifier,
                   const CanonicalSet set, const CanonicalSet rightSet)
    : operation(operation),
      identifier(std::move(identifier)),
      leftOperand(left),
      rightOperand(right),
      set(set),
      rightSet(rightSet) {}
CanonicalRelation Relation::fullRelation() {
  return newRelation(RelationOperation::fullRelation, nullptr, nullptr, std::nullopt, nullptr,
                     nullptr);
}
CanonicalRelation Relation::emptyRelation() {
  return newRelation(RelationOperation::emptyRelation, nullptr, nullptr, std::nullopt, nullptr,
                     nullptr);
}
CanonicalRelation Relation::idRelation() {
  return newRelation(RelationOperation::idRelation, nullptr, nullptr, std::nullopt, nullptr,
                     nullptr);
}
CanonicalRelation Relation::setIdentity(const CanonicalSet set) {
  return newRelation(RelationOperation::setIdentity, nullptr, nullptr, std::nullopt, set, nullptr);
}
CanonicalRelation Relation::cartesianProduct(const CanonicalSet left, const CanonicalSet right) {
  return newRelation(RelationOperation::cartesianProduct, nullptr, nullptr, std::nullopt, left,
                     right);
}
CanonicalRelation Relation::newBaseRelation(std::string identifier) {
  return newRelation(RelationOperation::baseRelation, nullptr, nullptr, identifier, nullptr,
                     nullptr);
}
CanonicalRelation Relation::newRelation(const RelationOperation operation,
                                        const CanonicalRelation left) {
  return newRelation(operation, left, nullptr, std::nullopt, nullptr, nullptr);
}
CanonicalRelation Relation::newRelation(const RelationOperation operation,
                                        const CanonicalRelation left,
                                        const CanonicalRelation right) {
  return newRelation(operation, left, right, std::nullopt, nullptr, nullptr);
}
bool Relation::operator==(const Relation& other) const {
  return operation == other.operation && leftOperand == other.leftOperand &&
         rightOperand == other.rightOperand && identifier == other.identifier && set == other.set;
}

CanonicalRelation Relation::newRelation(const RelationOperation operation,
                                        const CanonicalRelation left, const CanonicalRelation right,
                                        const std::optional<std::string>& identifier,
                                        const CanonicalSet set, const CanonicalSet rightSet) {
#if (DEBUG)
  // ------------------ Validation ------------------
  const bool isBinary = (left != nullptr && right != nullptr);
  const bool isUnary = (left == nullptr) == (right != nullptr);
  const bool isNullary = (left == nullptr && right == nullptr);
  const bool hasId = identifier.has_value();
  const bool hasSet = set != nullptr;
  const bool hasRightSet = rightSet != nullptr;
  switch (operation) {
    case RelationOperation::baseRelation:
      assert(hasId && isNullary && !hasSet && !hasRightSet);
      break;
    case RelationOperation::idRelation:
    case RelationOperation::emptyRelation:
    case RelationOperation::fullRelation:
      assert(!hasId && isNullary && !hasSet && !hasRightSet);
      break;
    case RelationOperation::relationUnion:
    case RelationOperation::relationIntersection:
    case RelationOperation::composition:
      assert(!hasId && isBinary && !hasSet && !hasRightSet);
      break;
    case RelationOperation::transitiveClosure:
    case RelationOperation::converse:
      assert(!hasId && isUnary && !hasSet && !hasRightSet);
      break;
    case RelationOperation::setIdentity:
      assert(!hasId && isNullary && hasSet && !hasRightSet);
      break;
    case RelationOperation::cartesianProduct:
      assert(!hasId && isNullary && hasSet && hasRightSet);
      break;
    default:
      assert(false);
      throw std::logic_error("unreachable");
  }
#endif
  // optimizations
  switch (operation) {
    case RelationOperation::composition:
    case RelationOperation::relationIntersection:
      if (left->operation == RelationOperation::emptyRelation ||
          right->operation == RelationOperation::emptyRelation) {
        return emptyRelation();
      }
    case RelationOperation::baseRelation:
    case RelationOperation::idRelation:
    case RelationOperation::emptyRelation:
    case RelationOperation::fullRelation:
      break;
    case RelationOperation::relationUnion:
      if (left->operation == RelationOperation::emptyRelation &&
          right->operation == RelationOperation::emptyRelation) {
        return emptyRelation();
      }
      if (left->operation == RelationOperation::emptyRelation) {
        return right;
      }
      if (right->operation == RelationOperation::emptyRelation) {
        return left;
      }
      break;
    case RelationOperation::converse:
      if (left->operation == RelationOperation::emptyRelation) {
        return emptyRelation();
      }
      break;
    case RelationOperation::transitiveClosure:
      if (left->operation == RelationOperation::emptyRelation) {
        return idRelation();
      }
      break;
    case RelationOperation::setIdentity:
      if (set->operation == SetOperation::emptySet) {
        return emptyRelation();
      }
    case RelationOperation::cartesianProduct:
      break;
  }
  static std::unordered_set<Relation> canonicalizer;
  auto [iter, created] = canonicalizer.emplace(operation, left, right, identifier, set, rightSet);
  Stats::boolean("#relations").count(created);
  return &(*iter);
}

CanonicalRelation Relation::converse() const {
  return newRelation(RelationOperation::converse, this);
}

CanonicalRelation Relation::composeWith(const CanonicalRelation other) const {
  return newRelation(RelationOperation::composition, this, other);
}

CanonicalRelation Relation::intersectWith(const CanonicalRelation other) const {
  return newRelation(RelationOperation::relationIntersection, this, other);
}

int Relation::intersectionWidth() const {
  switch (operation) {
    case RelationOperation::relationIntersection:
      return leftOperand->intersectionWidth() + rightOperand->intersectionWidth();
    case RelationOperation::composition:
    case RelationOperation::relationUnion:
      return std::max(leftOperand->intersectionWidth(), rightOperand->intersectionWidth());
    case RelationOperation::converse:
    case RelationOperation::transitiveClosure:
      return leftOperand->intersectionWidth();
    case RelationOperation::baseRelation:
    case RelationOperation::idRelation:
    case RelationOperation::emptyRelation:
    case RelationOperation::fullRelation:
    case RelationOperation::setIdentity:       // TODO:
    case RelationOperation::cartesianProduct:  // TODO:
      return 1;
    default:
      throw std::logic_error("unreachable");
  }
}

int Relation::compositionLength() const {
  switch (operation) {
    case RelationOperation::composition:
      return leftOperand->compositionLength() + rightOperand->compositionLength();
    case RelationOperation::relationIntersection:
    case RelationOperation::relationUnion:
      return std::max(leftOperand->compositionLength(), rightOperand->compositionLength());
    case RelationOperation::converse:
    case RelationOperation::transitiveClosure:
      return leftOperand->compositionLength() + 1;
    case RelationOperation::baseRelation:
    case RelationOperation::emptyRelation:
    case RelationOperation::fullRelation:
    case RelationOperation::setIdentity:       // TODO:
    case RelationOperation::cartesianProduct:  // TODO:
      return 1;
    case RelationOperation::idRelation:
      return 0;
    default:
      throw std::logic_error("unreachable");
  }
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
      output += set->toString() + "x" + rightSet->toString();
      break;
    default:
      throw std::logic_error("unreachable");
  }
  return output;
}

bool Relation::isSmallerReason(const CanonicalRelation other) const {
  if (other == nullptr) {
    return true;
  }
  auto lWidth = intersectionWidth();
  auto rWidth = other->intersectionWidth();
  auto lLength = compositionLength();
  auto rLength = other->compositionLength();
  return std::tie(lWidth, lLength) < std::tie(rWidth, rLength);
}

std::size_t std::hash<RelationOperation>::operator()(
    const RelationOperation& operation) const noexcept {
  return static_cast<std::size_t>(operation);
}

std::size_t std::hash<Relation>::operator()(const Relation& relation) const noexcept {
  const size_t opHash = hash<RelationOperation>()(relation.operation);
  const size_t leftHash = hash<CanonicalRelation>()(relation.leftOperand);
  const size_t rightHash = hash<CanonicalRelation>()(relation.rightOperand);
  const size_t idHash = hash<optional<std::string>>()(relation.identifier);
  const size_t setHash = hash<CanonicalSet>()(relation.set);

  return ((opHash ^ leftHash << 1) >> 1 ^ rightHash << 1) + idHash ^ (setHash << 28);
}
