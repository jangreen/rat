#pragma once
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ProofRule.h"
#include "Set.h"

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

  explicit Predicate(const std::string &expression);  // parse constructor
  Predicate(const PredicateOperation operation);
  Predicate(const PredicateOperation operation, Set &&left, Set &&right);

  PredicateOperation operation;
  std::unique_ptr<Set> leftOperand;   // is set iff binary predicate
  std::unique_ptr<Set> rightOperand;  // is set iff binary predicate

  // printing
  std::string toString() const;
};
