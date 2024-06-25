#pragma once
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "Relation.h"

// forward declaration
class Literal;
class Set;

typedef std::vector<Literal> Cube;
typedef std::vector<Cube> DNF;
typedef std::vector<int> Renaming;
typedef std::vector<int> Renaming;
typedef const Set *CanonicalSet;
// a PartialPredicate can be either a Predicate or a Set that will be used to construct a
// predicate for a given context
typedef std::variant<CanonicalSet, Literal> PartialLiteral;
typedef std::vector<PartialLiteral> PartialCube;
typedef std::vector<PartialCube> PartialDNF;

enum class PredicateOperation {
  edge,            // (e1, e2) \in a
  set,             // e1 \in A
  equality,        // e1 = e2
  setNonEmptiness  // s != 0
};

class Literal {
 public:
  Literal(bool negated, CanonicalSet set);
  Literal(bool negated, int leftLabel, std::string identifier);
  Literal(bool negated, int leftLabel, int rightLabel, std::string identifier);
  Literal(bool negated, int leftLabel, int rightLabel);
  bool validate() const;
  // Literal(const Literal &&other) = delete;
  // Literal(const Literal &other) = default;
  // Literal &operator=(const Literal &other) = default;

  bool operator==(const Literal &other) const;
  bool operator<(const Literal &other) const;  // for sorting/hashing
  bool isNegatedOf(const Literal &other) const;

  bool negated;
  PredicateOperation operation;
  CanonicalSet set;                       // setNonEmptiness
  std::optional<int> leftLabel;           // edge, set, equality
  std::optional<int> rightLabel;          // edge, equality
  std::optional<std::string> identifier;  // edge, set

  [[nodiscard]] bool isNormal() const;
  [[nodiscard]] bool hasTopSet() const;
  [[nodiscard]] bool isPositiveEdgePredicate() const;
  [[nodiscard]] bool isPositiveEqualityPredicate() const;
  [[nodiscard]] std::vector<int> labels() const;
  [[nodiscard]] std::vector<CanonicalSet> labelBaseCombinations() const;

  [[nodiscard]] std::optional<DNF> applyRule(bool modalRules) const;
  // substitute n-th occurrence, TODO: -1 = all
  bool substitute(CanonicalSet search, CanonicalSet replace, int n);
  Literal substituteSet(CanonicalSet set) const;
  void rename(const Renaming &renaming, bool inverse);

  static int saturationBoundId;
  static int saturationBoundBase;
  int saturatedId;
  int saturatedBase;
  void saturateId();
  void saturateBase();

  // printing
  [[nodiscard]] std::string toString() const;

  static void print(const DNF &dnf) {
    std::cout << "Cubes:";
    for (auto &cube : dnf) {
      std::cout << "\n";
      for (auto &literal : cube) {
        std::cout << literal.toString() << " , ";
      }
    }
    std::cout << std::endl;
  }

  static void print(const Cube &cube) {
    for (auto &literal : cube) {
      std::cout << literal.toString() << " , ";
    }
    std::cout << std::endl;
  }

  static int rename(int n, const Renaming &renaming, bool inverse) {
    return inverse ? renaming[n] : std::distance(renaming.begin(), std::ranges::find(renaming, n));
  }
};

static const Literal BOTTOM = Literal(false, -1, -1);
static const Literal TOP = Literal(true, -1, -1);

//---------------- Set.h

enum class SetOperation {
  base,          // nullary function (constant): base Set
  singleton,     // nullary function (constant): single Set
  empty,         // nullary function (constant): empty Set
  full,          // nullary function (constant): full Set
  choice,        // binary function
  intersection,  // binary function
  image,         // binary function
  domain
};

enum class RuleDirection { Left, Right };

class Set {
 private:
  static CanonicalSet newSet(SetOperation operation, CanonicalSet left, CanonicalSet right,
                             CanonicalRelation relation, std::optional<int> label,
                             const std::optional<std::string> &identifier);

  // Cached values
  mutable std::optional<std::string> cachedStringRepr;

  // properties calculated for canonical sets on initialization
  mutable bool _isNormal{};
  mutable bool _hasTopSet{};
  mutable std::vector<int> labels;
  mutable std::vector<CanonicalSet> labelBaseCombinations;

  // Calculates the above properties: we do not do this inside the constructor
  //  to avoid doing it for non-canonical sets.
  void completeInitialization() const;

 public:
  Set(SetOperation operation, CanonicalSet left, CanonicalSet right, CanonicalRelation relation,
      std::optional<int> label, std::optional<std::string> identifier);  // do not use
  static int maxSingletonLabel;  // to create globally unique labels
  static CanonicalSet newSet(SetOperation operation);
  static CanonicalSet newSet(SetOperation operation, CanonicalSet left, CanonicalSet right);
  static CanonicalSet newSet(SetOperation operation, CanonicalSet left, CanonicalRelation relation);
  static CanonicalSet newEvent(int label);
  static CanonicalSet newBaseSet(std::string &identifier);

  Set(const Set &other) = delete;
  Set(const Set &&other) noexcept;  // used for try_emplace (do not want to use copy constructor)

  bool operator==(const Set &other) const;

  const bool &isNormal() const;
  const bool &hasTopSet() const;
  const std::vector<int> &getLabels() const;
  const std::vector<CanonicalSet> &getLabelBaseCombinations() const;

  const SetOperation operation;
  const std::optional<std::string> identifier;  // is set iff operation base
  const std::optional<int> label;               // is set iff operation singleton
  const CanonicalSet leftOperand;               // is set iff operation unary/binary
  const CanonicalSet rightOperand;              // is set iff operation binary
  const CanonicalRelation relation;             // is set iff domain/image

  [[nodiscard]] CanonicalSet rename(const Renaming &renaming, bool inverse) const;
  CanonicalSet substitute(CanonicalSet search, CanonicalSet replace, int *n) const;

  [[nodiscard]] std::optional<PartialDNF> applyRule(const Literal *context, bool modalRules) const;
  [[nodiscard]] CanonicalSet saturateId() const;
  [[nodiscard]] CanonicalSet saturateBase() const;

  // printing
  [[nodiscard]] std::string toString() const;
};

/// hashing
#include <boost/functional/hash.hpp>

template <>
struct std::hash<Literal> {
  std::size_t operator()(const Literal &literal) const noexcept {
    return ((hash<PredicateOperation>()(literal.operation) ^
             (hash<CanonicalSet>()(literal.set) << 1)) >>
            1) ^
           (hash<bool>()(literal.negated) << 1);
  }
};

template <>
struct std::hash<SetOperation> {
  std::size_t operator()(const SetOperation &operation) const noexcept {
    using std::hash;
    using std::size_t;
    using std::string;
    return static_cast<std::size_t>(operation);
  }
};

template <>
struct std::hash<Set> {
  std::size_t operator()(const Set &set) const noexcept {
    using std::hash;
    using std::size_t;
    using std::string;

    // Compute individual hash values for first,
    // second and third and combine them using XOR
    // and bit shifting:

    const size_t opHash = hash<SetOperation>()(set.operation);
    const size_t leftHash = hash<CanonicalSet>()(set.leftOperand);
    const size_t rightHash = hash<CanonicalSet>()(set.rightOperand);
    const size_t relHash = hash<CanonicalRelation>()(set.relation);
    const size_t idHash = hash<std::optional<std::string>>()(set.identifier);
    const size_t labelHash = hash<std::optional<int>>()(set.label);

    return (opHash ^ (leftHash << 1) >> 1) ^ (rightHash << 1) + 31 * relHash + idHash + labelHash;
  }
};