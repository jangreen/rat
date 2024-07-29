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
  explicit Literal(bool negated);                           // constant
  explicit Literal(CanonicalSet set);                       // positive setNonEmptiness
  explicit Literal(const AnnotatedSet &annotatedSet);       // negated setNonEmptiness
  Literal(CanonicalSet event, std::string identifier);      // positive set
  Literal(CanonicalSet event, std::string identifier,       //
          const AnnotationType &annotation);                // negated set
  Literal(CanonicalSet leftEvent, CanonicalSet rightEvent,  //
          std::string identifier);                          // positive edge
  Literal(CanonicalSet leftEvent, CanonicalSet rightEvent, std::string identifier,  //
          const AnnotationType &annotation);                                        // negated edge
  Literal(bool negated, CanonicalSet leftEvent, CanonicalSet rightEvent);           // equality
  [[nodiscard]] bool validate() const;

  std::strong_ordering operator<=>(const Literal &other) const;
  bool operator==(const Literal &other) const { return *this <=> other == 0; }
  [[nodiscard]] bool isNegatedOf(const Literal &other) const;

  bool negated;
  PredicateOperation operation;
  CanonicalSet set;                // setNonEmptiness
  CanonicalAnnotation annotation;  // negated + setNonEmptiness, edge
  CanonicalSet leftEvent;          // edge, set, equality
  CanonicalSet rightEvent;         // edge, equality
  // std::optional<std::string> identifier;  // edge, set
  std::optional<CanonicalString> identifier;  // edge, set

  [[nodiscard]] bool isNormal() const;
  // TODO (topEvent optimization): [[nodiscard]] bool hasTopEvent() const { return
  // !topEvents().empty(); };
  [[nodiscard]] bool hasFullSet() const {
    return operation == PredicateOperation::setNonEmptiness && set->hasFullSet();
  }
  [[nodiscard]] bool hasBaseSet() const {
    return operation == PredicateOperation::setNonEmptiness && set->hasBaseSet();
  }
  [[nodiscard]] bool isPositiveEdgePredicate() const {
    return !negated && operation == PredicateOperation::edge;
  }
  [[nodiscard]] bool isPositiveSetPredicate() const {
    return !negated && operation == PredicateOperation::set;
  }
  [[nodiscard]] bool isPositiveAtomic() const {
    return !negated && operation != PredicateOperation::setNonEmptiness;
  }
  [[nodiscard]] bool isPositiveEqualityPredicate() const {
    return !negated && operation == PredicateOperation::equality;
  }
  [[nodiscard]] EventSet normalEvents() const;
  [[nodiscard]] EventSet events() const;
  // TODO (topEvent optimization): [[nodiscard]] EventSet topEvents() const;
  [[nodiscard]] SetOfSets labelBaseCombinations() const;

  std::optional<Literal> substituteAll(CanonicalSet search, CanonicalSet replace) const;
  bool substitute(CanonicalSet search, CanonicalSet replace, int n);  // substitute n-th occurrence
  [[nodiscard]] Literal substituteSet(const AnnotatedSet &set) const;
  void rename(const Renaming &renaming);
  [[nodiscard]] AnnotatedSet annotatedSet() const { return {set, annotation}; }

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
    const size_t leftLabelHash = hash<CanonicalSet>()(literal.leftEvent);
    const size_t rightLabelHash = hash<CanonicalSet>()(literal.leftEvent);
    return ((opHash ^ (setHash << 1)) >> 1) ^
           (signHash << 1) + 31 * idHash + 7 * leftLabelHash + rightLabelHash;
  }
};