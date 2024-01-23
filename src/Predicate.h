#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Relation.h"

// forward declareation
class Set;
class Formula;

typedef std::vector<int> Renaming;

enum class PredicateOperation {
  edge,                     // (e1, e2) \in a
  set,                      // e1 \in A
  equality,                 // e1 = e2
  intersectionNonEmptiness  // binary predicate
};

class Predicate {
 public:
  /* Rule of five */
  Predicate(const Predicate &other);
  Predicate(Predicate &&other) = default;
  Predicate &operator=(const Predicate &other);
  Predicate &operator=(Predicate &&other) = default;
  ~Predicate() = default;
  friend void swap(Predicate &first, Predicate &second) {
    using std::swap;
    swap(first.operation, second.operation);
    swap(first.leftOperand, second.leftOperand);
    swap(first.rightOperand, second.rightOperand);
    swap(first.identifier, second.identifier);
  }
  bool operator==(const Predicate &other) const;

  explicit Predicate(const std::string &expression);  // parse constructor
  Predicate(const PredicateOperation operation, Set &&left, Set &&right);
  Predicate(const PredicateOperation operation, Set &&left, std::string identifier);
  Predicate(const PredicateOperation operation, Set &&left, Set &&right, std::string identifier);

  PredicateOperation operation;
  std::unique_ptr<Set> leftOperand;       // is set iff binary predicate
  std::unique_ptr<Set> rightOperand;      // is set iff binary predicate
  std::optional<std::string> identifier;  // is set iff edge/set predicate

  std::optional<Formula> applyRule(bool modalRules = false);
  int substitute(const Set &search, const Set &replace, int n);
  bool isNormal() const;
  std::vector<int> labels() const;
  void rename(const Renaming &renaming);  // renames given a renaming function

  void saturate();

  // printing
  std::string toString() const;
};

// ---------- SET header
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
 public:
  /* Rule of five */
  Set(const Set &other);
  Set(Set &&other) = default;
  Set &operator=(const Set &other);
  Set &operator=(Set &&other) = default;
  ~Set() = default;
  friend void swap(Set &first, Set &second) {
    using std::swap;
    swap(first.operation, second.operation);
    swap(first.identifier, second.identifier);
    swap(first.label, second.label);
    swap(first.leftOperand, second.leftOperand);
    swap(first.rightOperand, second.rightOperand);
    swap(first.relation, second.relation);
    /*swap(first.saturated, second.saturated);
    swap(first.saturatedId, second.saturatedId);*/
  }
  bool operator==(const Set &other) const;
  bool operator<(const Set &otherSet) const;  // for sorting/hashing

  explicit Set(const std::string &expression);  // parse constructor
  explicit Set(const SetOperation operation,
               const std::optional<std::string> &identifier = std::nullopt);
  explicit Set(const SetOperation operation, int label);
  Set(const SetOperation operation, Set &&left);
  Set(const SetOperation operation, Set &&left, Set &&right);
  Set(const SetOperation operation, Set &&left, Relation &&relation);

  SetOperation operation;
  std::optional<std::string> identifier;  // is set iff operation base
  std::optional<int> label;               // is set iff operation singleton
  std::unique_ptr<Set> leftOperand;       // is set iff operation unary/binary
  std::unique_ptr<Set> rightOperand;      // is set iff operation binary
  std::unique_ptr<Relation> relation;     // is set iff domain/image

  static int maxSingletonLabel;  // to create globally unique labels

  /* TODO: remove
  std::vector<int> labels() const;                // return all labels of the relation term
  std::vector<int> calculateRenaming() const;     // renaming {2,4,5}: 2->0,4->1,5->2
  void inverseRename(const std::vector<int> &renaming);
  */

  // a PartialPredicate can be either a Predicate or a Set that will be used to construct a
  // predicate for a given context
  typedef std::variant<Set, Predicate> PartialPredicate;
  std::optional<std::vector<std::vector<PartialPredicate>>> applyRule(bool modalRules = false);
  int substitute(const Set &search, const Set &replace, int n);  // returns new n (1 = replace)
  bool isNormal() const;  // true iff all labels are in front of base Sets
  std::vector<int> labels() const;
  void rename(const Renaming &renaming);  // renames given a renaming function

  void saturate();

  // printing
  std::string toString() const;
};
