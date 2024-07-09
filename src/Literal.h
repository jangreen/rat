#pragma once
#include <optional>
#include <string>
#include <vector>

#include "Annotated.h"
#include "CanonicalString.h"
#include "Relation.h"
#include "Renaming.h"
#include "Set.h"

// forward declaration
class Literal;
typedef std::vector<Literal> Cube;
typedef std::vector<Cube> DNF;
typedef std::variant<AnnotatedSet, Literal> PartialLiteral;
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
 public:
  explicit Literal(bool negated);                                  // constant
  explicit Literal(CanonicalSet set);                              // positive setNonEmptiness
  explicit Literal(const AnnotatedSet &annotatedSet);              // negative setNonEmptiness
  Literal(bool negated, int leftLabel, std::string identifier);    // set
  Literal(int leftLabel, int rightLabel, std::string identifier);  // positive edge
  Literal(int leftLabel, int rightLabel, std::string identifier,   //
          AnnotationType annotation);                              // negative edge
  Literal(bool negated, int leftLabel, int rightLabel);            // equality
  [[nodiscard]] bool validate() const;

  std::strong_ordering operator<=>(const Literal &other) const;
  bool operator==(const Literal &other) const { return *this <=> other == 0; }
  [[nodiscard]] bool isNegatedOf(const Literal &other) const;

  bool negated;
  PredicateOperation operation;
  CanonicalSet set;                // setNonEmptiness
  CanonicalAnnotation annotation;  // negated + setNonEmptiness, edge
  std::optional<int> leftLabel;    // edge, set, equality
  std::optional<int> rightLabel;   // edge, equality
  // std::optional<std::string> identifier;  // edge, set
  std::optional<CanonicalString> identifier;  // edge, set

  [[nodiscard]] bool isNormal() const;
  [[nodiscard]] bool hasTopEvent() const;
  [[nodiscard]] bool isPositiveEdgePredicate() const;
  [[nodiscard]] bool isPositiveEqualityPredicate() const;
  [[nodiscard]] EventSet events() const;
  [[nodiscard]] EventSet topEvents() const;
  [[nodiscard]] SetOfSets labelBaseCombinations() const;

  std::optional<Literal> substituteAll(const CanonicalSet search, const CanonicalSet replace) const;
  bool substitute(CanonicalSet search, CanonicalSet replace, int n);  // substitute n-th occurrence
  Literal substituteSet(const AnnotatedSet &set) const;
  void rename(const Renaming &renaming);
  inline AnnotatedSet annotatedSet() const { return AnnotatedSet(set, annotation); };

  // printing
  [[nodiscard]] std::string toString() const;
};

static const Literal BOTTOM = Literal(true);
static const Literal TOP = Literal(false);

/// hashing
#include <boost/functional/hash.hpp>

template <>
struct std::hash<Literal> {
  std::size_t operator()(const Literal &literal) const noexcept {
    const size_t opHash = hash<PredicateOperation>()(literal.operation);
    const size_t setHash = hash<CanonicalSet>()(literal.set);  // Hashes the pointer
    const size_t signHash = hash<bool>()(literal.negated);
    const size_t idHash = hash<std::optional<std::string>>()(literal.identifier);
    const size_t leftLabelHash = hash<std::optional<int>>()(literal.leftLabel);
    const size_t rightLabelHash = hash<std::optional<int>>()(literal.leftLabel);
    return ((opHash ^ (setHash << 1)) >> 1) ^
           (signHash << 1) + 31 * idHash + 7 * leftLabelHash + rightLabelHash;
  }
};

template <>
struct std::hash<SetOperation> {
  std::size_t operator()(const SetOperation &operation) const noexcept {
    return static_cast<std::size_t>(operation);
  }
};

template <>
struct std::hash<Set> {
  std::size_t operator()(const Set &set) const noexcept {
    const size_t opHash = hash<SetOperation>()(set.operation);
    const size_t leftHash = hash<CanonicalSet>()(set.leftOperand);
    const size_t rightHash = hash<CanonicalSet>()(set.rightOperand);
    const size_t relHash = hash<CanonicalRelation>()(set.relation);
    const size_t idHash = hash<std::optional<std::string>>()(set.identifier);
    const size_t labelHash = hash<std::optional<int>>()(set.label);

    return (opHash ^ (leftHash << 1) >> 1) ^ (rightHash << 1) + 31 * relHash + idHash + labelHash;
  }
};