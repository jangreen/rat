#pragma once
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ProofRule.h"
#include "Relation.h"

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
    swap(first.saturated, second.saturated);
    swap(first.saturatedId, second.saturatedId);
  }

  explicit Set(const std::string &expression);  // parse constructor
  explicit Set(const SetOperation operation,
               const std::optional<std::string> &iodentifier = std::nullopt);
  explicit Set(const SetOperation operation, int label);
  Set(const SetOperation operation, Set &&left);
  Set(const SetOperation operation, Set &&left, Set &&right);
  Set(const SetOperation operation, Set &&left, Relation &&right);

  SetOperation operation;
  std::optional<std::string> identifier;  // is set iff operation base
  std::optional<int> label;               // is set iff operation singleton
  std::unique_ptr<Set> leftOperand;       // is set iff operation unary/binary
  std::unique_ptr<Set> rightOperand;      // is set iff operation binary
  std::unique_ptr<Relation> relation;     // is set iff domain/image
  bool saturated = false;                 // mark base Set
  bool saturatedId = false;

  static int maxSingletonLabel;  // to create globally unique labels

  bool isNormal() const;                       // true iff all labels are in front of base Sets
  bool operator==(const Set &otherSet) const;  // compares two Set syntactically
  bool operator<(const Set &otherSet) const;   // for sorting/hashing
  // printing
  std::string toString() const;
};
