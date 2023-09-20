#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Relation.h"

// forward declareation
class Set;
class Formula;

// TODO: move bottom, top into FormulaOperation?
enum class PredicateOperation {
  bottom,                   // nullary predicate
  top,                      // nullary predicate
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
  }
  bool operator==(const Predicate &other) const;

  explicit Predicate(const std::string &expression);  // parse constructor
  Predicate(const PredicateOperation operation);
  Predicate(const PredicateOperation operation, Set &&left, Set &&right);

  PredicateOperation operation;
  std::unique_ptr<Set> leftOperand;   // is set iff binary predicate
  std::unique_ptr<Set> rightOperand;  // is set iff binary predicate

  std::optional<Formula> applyRule(bool modalRules = false);
  bool isNormal() const;
  bool isAtomic() const;

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
    swap(first.leftOperand, second.leftOperand);
    swap(first.rightOperand, second.rightOperand);
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

  /*
  std::vector<int> labels() const;                // return all labels of the relation term
  std::vector<int> calculateRenaming() const;     // renaming {2,4,5}: 2->0,4->1,5->2
  void rename(const std::vector<int> &renaming);  // renames given a renaming function
  void inverseRename(const std::vector<int> &renaming);
*/

  // a PartialPredicate can be either a Predicate or a Set that will be used to construct a
  // predicate for a given context
  typedef std::variant<Set, Predicate> PartialPredicate;
  std::optional<std::vector<std::vector<PartialPredicate>>> applyRule(bool modalRules = false);
  bool isNormal() const;  // true iff all labels are in front of base Sets

  // printing
  std::string toString() const;
};

// LEGACY
/*bool saturated = false;                 // mark base Set
  bool saturatedId = false;*/

// this function tries to apply a rule deep inside
// returns  - false if rule is not applicable
//          - result otherwise
/*template <ProofRule::Rule rule>
std::optional<GDNF> applyRuleDeep() {
  auto baseCase = applyRule<rule>();
  if (baseCase) {
    return *baseCase;
  }
  // case: intersection or composition (only cases where we apply rules deep inside)
  if (operation == SetOperation::composition || operation == RelationOperation::intersection) {
    auto leftResult = leftOperand->applyRuleDeep<rule>();
    if (leftResult) {
      return substituteLeft(std::move(*leftResult));
    }
    // case: intersection
    if (operation == RelationOperation::intersection) {
      auto rightResult = rightOperand->applyRuleDeep<rule>();
      if (rightResult) {
        return substituteRight(std::move(*rightResult));
      }
    }
  }
  return std::nullopt;
}
GDNF substituteLeft(GDNF &&newLeft);
GDNF substituteRight(GDNF &&newRight);*/