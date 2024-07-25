#pragma once
#include <optional>
#include <string>

// TODO: merge with Set

// forward declaration
class Set;
class Relation;
typedef const Relation *CanonicalRelation;
typedef const Set *CanonicalSet;

enum class RelationOperation {
  baseRelation,          // nullary function (constant): base relation
  idRelation,            // nullary function (constant): identity relation
  emptyRelation,         // nullary function (constant): empty relation
  fullRelation,          // nullary function (constant): full relation
  relationUnion,         // binary function
  relationIntersection,  // binary function
  composition,           // binary function
  transitiveClosure,     // unary function
  converse,              // unary function
  setIdentity,           // unary function
  cartesianProduct       // TODO:
};

class Relation {
 private:
  static CanonicalRelation newRelation(RelationOperation operation, CanonicalRelation left,
                                       CanonicalRelation right,
                                       const std::optional<std::string> &identifier,
                                       CanonicalSet set);

 public:
  // WARNING: Never call this constructor: it is only public for technicaly reasons
  Relation(RelationOperation operation, CanonicalRelation left, CanonicalRelation right,
           std::optional<std::string> identifier, CanonicalSet set);

  static CanonicalRelation fullRelation() {
    return newRelation(RelationOperation::fullRelation, nullptr, nullptr, std::nullopt, nullptr);
  }
  static CanonicalRelation emptyRelation() {
    return newRelation(RelationOperation::emptyRelation, nullptr, nullptr, std::nullopt, nullptr);
  }
  static CanonicalRelation idRelation() {
    return newRelation(RelationOperation::idRelation, nullptr, nullptr, std::nullopt, nullptr);
  }
  static CanonicalRelation setIdentity(const CanonicalSet set) {
    return newRelation(RelationOperation::setIdentity, nullptr, nullptr, std::nullopt, set);
  }
  static CanonicalRelation newBaseRelation(std::string identifier) {
    return newRelation(RelationOperation::baseRelation, nullptr, nullptr, identifier, nullptr);
  }
  static CanonicalRelation newRelation(const RelationOperation operation,
                                       const CanonicalRelation left) {
    return newRelation(operation, left, nullptr, std::nullopt, nullptr);
  }
  static CanonicalRelation newRelation(const RelationOperation operation,
                                       const CanonicalRelation left,
                                       const CanonicalRelation right) {
    return newRelation(operation, left, right, std::nullopt, nullptr);
  };

  Relation(const Relation &other) = delete;
  Relation(const Relation &&other) = delete;

  bool operator==(const Relation &other) const {
    return operation == other.operation && leftOperand == other.leftOperand &&
           rightOperand == other.rightOperand && identifier == other.identifier;
  }

  const RelationOperation operation;
  const std::optional<std::string> identifier;  // is set iff operation base
  const CanonicalRelation leftOperand;          // is set iff operation unary/binary
  const CanonicalRelation rightOperand;         // is set iff operation binary
  const CanonicalSet set;                       // is set iff operation setIdentity

  [[nodiscard]] std::string toString() const;
};

/// hashing

template <>
struct std::hash<RelationOperation> {
  std::size_t operator()(const RelationOperation &operation) const noexcept {
    return static_cast<std::size_t>(operation);
  }
};

template <>
struct std::hash<Relation> {
  std::size_t operator()(const Relation &relation) const noexcept {
    const size_t opHash = hash<RelationOperation>()(relation.operation);
    const size_t leftHash = hash<CanonicalRelation>()(relation.leftOperand);
    const size_t rightHash = hash<CanonicalRelation>()(relation.rightOperand);
    const size_t idHash = hash<optional<std::string>>()(relation.identifier);
    const size_t setHash = hash<CanonicalSet>()(relation.set);

    return ((opHash ^ leftHash << 1) >> 1 ^ rightHash << 1) + idHash ^ (setHash << 28);
  }
};
