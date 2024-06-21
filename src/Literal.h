#pragma once
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Assumption.h"
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
typedef std::variant<CanonicalSet, Literal> PartialPredicate;

enum RuleDirection { Left, Right };

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
  Literal() = default;

  bool operator==(const Literal &other) const;
  bool operator<(const Literal &otherSet) const;  // for sorting/hashing

  bool negated{};
  PredicateOperation operation;
  CanonicalSet set{};                     // setNonEmptiness
  std::optional<int> leftLabel;           // edge, set, equality
  std::optional<int> rightLabel;          // edge, equality
  std::optional<std::string> identifier;  // edge, set

  [[nodiscard]] bool isNormal() const;
  [[nodiscard]] bool hasTopSet() const;
  [[nodiscard]] bool isPositiveEdgePredicate() const;
  [[nodiscard]] bool isPositiveEqualityPredicate() const;
  [[nodiscard]] std::vector<int> labels() const;
  [[nodiscard]] std::vector<CanonicalSet> labelBaseCombinations() const;

  std::optional<DNF> applyRule(bool modalRules);
  bool substitute(CanonicalSet search, CanonicalSet replace,
                  int n);  // substitute n-th occurence
  void rename(const Renaming &renaming, bool inverse);

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
    return inverse
               ? renaming[n]
               : std::distance(renaming.begin(), std::find(renaming.begin(), renaming.end(), n));
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

class Set {
 private:
  static CanonicalSet newSet(SetOperation operation, CanonicalSet left, CanonicalSet right,
                             CanonicalRelation relation, std::optional<int> label,
                             const std::optional<std::string> &identifier);

 public:
  Set(SetOperation operation, CanonicalSet left, CanonicalSet right, CanonicalRelation relation,
      std::optional<int> label, std::optional<std::string> identifier);  // do not use
  static int maxSingletonLabel;  // to create globally unique labels
  static std::unordered_map<Set, const Set> canonicalSets;
  static CanonicalSet newSet(SetOperation operation);
  static CanonicalSet newSet(SetOperation operation, CanonicalSet left);  // FIXME: Unused
  static CanonicalSet newSet(SetOperation operation, CanonicalSet left, CanonicalSet right);
  static CanonicalSet newSet(SetOperation operation, CanonicalSet left, CanonicalRelation relation);
  static CanonicalSet newEvent(int label);
  static CanonicalSet newBaseSet(std::string &identifier);

  Set(const Set &other) = delete;
  Set(const Set &&other) noexcept;  // used for try_emplace (do not want to use copy constructor)

  bool operator==(const Set &other) const;

  const SetOperation operation;
  const std::optional<std::string> identifier;  // is set iff operation base
  const std::optional<int> label;               // is set iff operation singleton
  CanonicalSet const leftOperand;               // is set iff operation unary/binary
  CanonicalSet const rightOperand;              // is set iff operation binary
  CanonicalRelation const relation;             // is set iff domain/image

  // properties calculated for canonical sets
  const bool isNormal;
  const bool hasTopSet;
  const std::vector<int> labels;
  const std::vector<CanonicalSet> labelBaseCombinations;

  [[nodiscard]] CanonicalSet rename(const Renaming &renaming, bool inverse) const;
  CanonicalSet substitute(CanonicalSet search, CanonicalSet replace, int *n) const;

  [[nodiscard]] std::optional<std::vector<std::vector<PartialPredicate>>> applyRule(
      bool negated, bool modalRules) const;
  [[nodiscard]] CanonicalSet saturateId() const;
  [[nodiscard]] CanonicalSet saturateBase() const;

  // FIXME: Both unused
  int saturatedId = 0;  // mark base relation
  int saturatedBase = 0;

  // printing
  [[nodiscard]] std::string toString() const;
};

/// hashing

template <>
struct std::hash<SetOperation> {
  std::size_t operator()(const SetOperation &operation) const {
    using std::hash;
    using std::size_t;
    using std::string;
    return static_cast<std::size_t>(operation);
  }
};

template <>
struct std::hash<Set> {
  std::size_t operator()(const Set &set) const {
    using std::hash;
    using std::size_t;
    using std::string;

    // Compute individual hash values for first,
    // second and third and combine them using XOR
    // and bit shifting:

    return ((hash<SetOperation>()(set.operation) ^ (hash<CanonicalSet>()(set.leftOperand) << 1)) >>
            1) ^
           (hash<CanonicalSet>()(set.rightOperand) << 1);
  }
};