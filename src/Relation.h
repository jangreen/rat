#pragma once
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// forward declaration
class Relation;
typedef const Relation *CanonicalRelation;

enum class RelationOperation {
  base,               // nullary function (constant): base relation
  identity,           // nullary function (constant): identity relation
  empty,              // nullary function (constant): empty relation
  full,               // nullary function (constant): full relation
  choice,             // binary function
  intersection,       // binary function
  composition,        // binary function
  transitiveClosure,  // unary function
  converse,           // unary function
  cartesianProduct    // TODO:
};

class Relation {
 private:
  static CanonicalRelation newRelation(RelationOperation operation, CanonicalRelation left,
                                       CanonicalRelation right,
                                       const std::optional<std::string>& identifier);

 public:
  Relation(RelationOperation operation, CanonicalRelation left, CanonicalRelation right,
           std::optional<std::string> identifier);  // do not use directly
  static std::unordered_map<Relation, const Relation> canonicalRelations;
  static CanonicalRelation newRelation(RelationOperation operation);
  static CanonicalRelation newRelation(RelationOperation operation, CanonicalRelation left);
  static CanonicalRelation newRelation(RelationOperation operation, CanonicalRelation left,
                                       CanonicalRelation right);
  static CanonicalRelation newBaseRelation(std::string identifier);

  Relation(const Relation &other) = delete;
  Relation(const Relation &&other) noexcept ;  // used for try_emplace (do not want to use copy constructor)

 public:
  bool operator==(const Relation &other) const;

  const RelationOperation operation;
  const std::optional<std::string> identifier;  // is set iff operation base
  CanonicalRelation const leftOperand;          // is set iff operation unary/binary
  CanonicalRelation const rightOperand;         // is set iff operation binary

  [[nodiscard]] std::string toString() const;
};

/// hashing

template <>
struct std::hash<RelationOperation> {
  std::size_t operator()(const RelationOperation &operation) const {
    using std::hash;
    using std::size_t;
    using std::string;
    return static_cast<std::size_t>(operation);
  }
};

template <>
struct std::hash<Relation> {
  std::size_t operator()(const Relation &relation) const {
    using std::hash;
    using std::size_t;
    using std::string;

    // Compute individual hash values for first,
    // second and third and combine them using XOR
    // and bit shifting:

    return ((hash<RelationOperation>()(relation.operation) ^
             (hash<CanonicalRelation>()(relation.leftOperand) << 1)) >>
            1) ^
           (hash<CanonicalRelation>()(relation.rightOperand) << 1);
  }
};
