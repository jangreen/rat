#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Literal.h"

enum class FormulaOperation {
  literal,    // nullary Formula
  negation,   // oneary Formula
  logicalOr,  // binary Formula
  logicalAnd
};

class Formula {
 public:
  /* Rule of five */
  Formula(const Formula &other);
  Formula(Formula &&other) = default;
  Formula &operator=(const Formula &other);
  Formula &operator=(Formula &&other) = default;
  ~Formula() = default;
  friend void swap(Formula &first, Formula &second) {
    using std::swap;
    swap(first.operation, second.operation);
    swap(first.leftOperand, second.leftOperand);
    swap(first.rightOperand, second.rightOperand);
  }
  bool operator==(const Formula &other) const;

  explicit Formula(const std::string &expression);  // parse constructor
  Formula(const FormulaOperation operation, Literal &&literal);
  Formula(const FormulaOperation operation, Formula &&left);
  Formula(const FormulaOperation operation, Formula &&left, Formula &&right);

  FormulaOperation operation;
  std::unique_ptr<Formula> leftOperand;   // is set iff oneary/binary Formula
  std::unique_ptr<Formula> rightOperand;  // is set iff binary Formula
  std::unique_ptr<Literal> literal;       // is set iff literal

  // functions for rule applications
  std::optional<std::vector<std::vector<Formula>>> applyRule();

  // printing
  std::string toString() const;
};

typedef std::vector<Formula> FormulaSet;
typedef std::vector<FormulaSet> GDNF;