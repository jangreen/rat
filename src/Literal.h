#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Predicate.h"

class Literal {
 public:
  /* Rule of five */
  Literal(const Literal &other);
  Literal(Literal &&other) = default;
  Literal &operator=(const Literal &other);
  Literal &operator=(Literal &&other) = default;
  ~Literal() = default;
  friend void swap(Literal &first, Literal &second) {
    using std::swap;
    swap(first.negated, second.negated);
    swap(first.predicate, second.predicate);
  }

  Literal(const bool negated, Predicate &&predicate);

  bool negated;
  std::unique_ptr<Predicate> predicate;

  std::optional<Formula> applyRule();

  // printing
  std::string toString() const;
};

typedef std::vector<Literal> Clause;
typedef std::vector<Clause> DNF;
