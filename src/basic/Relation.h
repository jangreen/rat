#pragma once

#include <boost/container/flat_set.hpp>
#include <optional>
#include <string>

#include "RelationOperation.h"

// TODO: merge with Set
// forward declaration
class Set;
class Relation;
typedef const Relation *CanonicalRelation;
typedef const Set *CanonicalSet;
typedef std::variant<CanonicalSet, CanonicalRelation> CanonicalExpression;
typedef boost::container::flat_set<CanonicalRelation> RelationsSet;
typedef boost::container::flat_set<CanonicalExpression> ExprSet;

class Relation {
  static CanonicalRelation newRelation(RelationOperation operation, CanonicalRelation left,
                                       CanonicalRelation right,
                                       const std::optional<std::string> &identifier,
                                       CanonicalSet set, CanonicalSet rightSet);

 public:
  // WARNING: Never call this constructor: it is only public for technicaly reasons
  Relation(RelationOperation operation, CanonicalRelation left, CanonicalRelation right,
           std::optional<std::string> identifier, CanonicalSet set, CanonicalSet rightSet);
  Relation(const Relation &other) = delete;
  Relation(const Relation &&other) = delete;

  static CanonicalRelation fullRelation();
  static CanonicalRelation emptyRelation();
  static CanonicalRelation idRelation();
  static CanonicalRelation setIdentity(CanonicalSet set);
  static CanonicalRelation cartesianProduct(CanonicalSet left, CanonicalSet right);
  static CanonicalRelation newBaseRelation(std::string identifier);
  static CanonicalRelation newRelation(RelationOperation operation, CanonicalRelation left);
  static CanonicalRelation newRelation(RelationOperation operation, CanonicalRelation left,
                                       CanonicalRelation right);

  const RelationOperation operation;
  const std::optional<std::string> identifier;  // is set iff operation base
  const CanonicalRelation leftOperand;          // is set iff operation unary/binary
  const CanonicalRelation rightOperand;         // is set iff operation binary
  const CanonicalSet set;       // is set iff operation setIdentity, cartesianProduct
  const CanonicalSet rightSet;  // is set iff operation cartesianProduct

  [[nodiscard]] CanonicalRelation converse() const;
  [[nodiscard]] CanonicalRelation composeWith(CanonicalRelation other) const;
  [[nodiscard]] CanonicalRelation intersectWith(CanonicalRelation other) const;
  [[nodiscard]] int intersectionWidth() const;
  [[nodiscard]] int compositionLength() const;
  [[nodiscard]] bool operator==(const Relation &other) const;
  [[nodiscard]] std::string toString() const;
  [[nodiscard]] bool isSmallerReason(CanonicalRelation other) const;
};

template <>
struct std::hash<Relation> {
  std::size_t operator()(const Relation &relation) const noexcept;
};
