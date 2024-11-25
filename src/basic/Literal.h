#pragma once
#include <optional>
#include <string>
#include <vector>

#include "CanonicalString.h"
#include "Relation.h"
#include "Renaming.h"
#include "Set.h"
#include "annotations/LeafAnnotated.h"
#include "annotations/ReasonAnnotation.h"

// forward declaration
class Literal;
typedef std::vector<Literal> Cube;
typedef std::vector<Cube> DNF;
typedef std::vector<LeafAnnotatedSet<Reasons>> SetCube;
typedef std::variant<LeafAnnotatedSet<Reasons>, Literal> PartialLiteral;
typedef std::vector<PartialLiteral> PartialCube;
typedef std::vector<PartialCube> PartialDNF;

// a PartialPredicate can be either a Predicate or a Set that will be used to construct a
// predicate for a given context

enum class PredicateOperation {
  edge,             // (e1, e2) \in a
  set,              // e1 \in A
  equality,         // e1 = e2
  setNonEmptiness,  // s != 0
  constant,         // true or false
};

class Literal {
  Literal(bool negated, PredicateOperation operation, CanonicalSet set,
          CanonicalLeafAnnotation<Reasons> annotation, CanonicalSet leftEvent,
          CanonicalSet rightEvent, std::optional<CanonicalString> identifier);

 public:
  static Literal newSetNonEmptiness(
      bool negated, CanonicalSet set,
      CanonicalLeafAnnotation<Reasons> annotation = LeafAnnotation<Reasons>::newLeaf({}));
  static Literal newSetMembership(
      bool negated, CanonicalSet event, CanonicalString identifier,
      CanonicalLeafAnnotation<Reasons> annotation = LeafAnnotation<Reasons>::newLeaf({}));
  static Literal newRelationMembership(
      bool negated, CanonicalSet leftEvent, CanonicalSet rightEvent, CanonicalString identifier,
      CanonicalLeafAnnotation<Reasons> annotation = LeafAnnotation<Reasons>::newLeaf({}));
  static Literal newEquality(
      bool negated, CanonicalSet leftEvent, CanonicalSet rightEvent,
      CanonicalLeafAnnotation<Reasons> annotation = LeafAnnotation<Reasons>::newLeaf({}));
  static Literal BOTTOM();
  static Literal TOP();
  // positive edge
  [[nodiscard]] bool validate() const;

  [[nodiscard]] std::strong_ordering operator<=>(const Literal &other) const;
  [[nodiscard]] bool operator==(const Literal &other) const { return *this <=> other == 0; }
  [[nodiscard]] bool isNegatedOf(const Literal &other) const;

  bool negated;
  PredicateOperation operation;
  CanonicalSet set;                                     // setNonEmptiness
  mutable CanonicalLeafAnnotation<Reasons> annotation;  // negated + setNonEmptiness, edge
  CanonicalSet leftEvent;                               // edge, set, equality
  CanonicalSet rightEvent;                              // edge, equality
  std::optional<CanonicalString> identifier;            // edge, set

  [[nodiscard]] bool isNormal() const;
  [[nodiscard]] bool hasFullSet() const;
  [[nodiscard]] bool hasBaseSet() const;
  [[nodiscard]] bool isPositiveEdgePredicate() const;
  [[nodiscard]] bool isPositiveSetPredicate() const;
  [[nodiscard]] bool isNegatedAtomic() const;
  [[nodiscard]] bool isPositiveAtomic() const;
  [[nodiscard]] bool isPositiveEqualityPredicate() const;
  [[nodiscard]] EventSet normalEvents() const;
  [[nodiscard]] EventSet events() const;
  [[nodiscard]] SetOfSets baseSets() const;
  [[nodiscard]] SetOfSets eventBasePairs() const;
  [[nodiscard]] SetOfSets saturatedEventBasePairs() const;

  [[nodiscard]] std::optional<Literal> substituteAll(CanonicalSet search,
                                                     CanonicalSet replace) const;
  [[nodiscard]] std::optional<Literal> substituteAll(CanonicalRelation search,
                                                     CanonicalRelation replace) const;
  [[nodiscard]] bool substitute(CanonicalSet search, CanonicalSet replace,
                                int n);  // substitute n-th occurrence
  [[nodiscard]] Literal substituteSet(const LeafAnnotatedSet<Reasons> &set) const;
  [[nodiscard]] Cube saturate() const;
  void rename(const Renaming &renaming);
  [[nodiscard]] LeafAnnotatedSet<Reasons> annotatedSet() const { return {set, annotation}; }

  // printing
  [[nodiscard]] std::string toString() const;
};

/// hashing
#include <boost/functional/hash.hpp>

template <>
struct std::hash<Literal> {
  std::size_t operator()(const Literal &literal) const noexcept {
    const size_t opHash = hash<PredicateOperation>()(literal.operation);
    const size_t setHash = hash<CanonicalSet>()(literal.set);  // Hashes the pointer
    const size_t signHash = hash<bool>()(literal.negated);
    const size_t idHash = hash<std::optional<std::string>>()(literal.identifier);
    const size_t leftLabelHash = hash<CanonicalSet>()(literal.leftEvent);
    const size_t rightLabelHash = hash<CanonicalSet>()(literal.leftEvent);
    return ((opHash ^ (setHash << 1)) >> 1) ^
           (signHash << 1) + 31 * idHash + 7 * leftLabelHash + rightLabelHash;
  }
};