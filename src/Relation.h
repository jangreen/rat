#pragma once
#include <optional>
#include <string>

// TODO: merge with Set

// forward declaration
class Relation;
typedef const Relation *CanonicalRelation;

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
  cartesianProduct       // TODO:
};

class Relation {
 private:
  static CanonicalRelation newRelation(RelationOperation operation, CanonicalRelation left,
                                       CanonicalRelation right,
                                       const std::optional<std::string> &identifier);

 public:
  // WARNING: Never call this constructor: it is only public for technicaly reasons
  Relation(RelationOperation operation, CanonicalRelation left, CanonicalRelation right,
           std::optional<std::string> identifier);

  static CanonicalRelation fullRelation() {
    return newRelation(RelationOperation::fullRelation, nullptr, nullptr, std::nullopt);
  }
  static CanonicalRelation emptyRelation() {
    return newRelation(RelationOperation::emptyRelation, nullptr, nullptr, std::nullopt);
  }
  static CanonicalRelation idRelation() {
    return newRelation(RelationOperation::idRelation, nullptr, nullptr, std::nullopt);
  }
  static CanonicalRelation newBaseRelation(std::string identifier) {
    return newRelation(RelationOperation::baseRelation, nullptr, nullptr, identifier);
  }
  static CanonicalRelation newRelation(RelationOperation operation, CanonicalRelation left) {
    return newRelation(operation, left, nullptr, std::nullopt);
  }
  static CanonicalRelation newRelation(RelationOperation operation, CanonicalRelation left,
                                              CanonicalRelation right) {
    return newRelation(operation, left, right, std::nullopt);
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

    return ((opHash ^ leftHash << 1) >> 1 ^ rightHash << 1) + idHash;
  }
};
