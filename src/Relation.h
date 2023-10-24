#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>

enum class RelationOperation {
  base,               // nullary function (constant): base relation
  identity,           // nullary function (constant): identity relation
  empty,              // nullary function (constant): empty relation
  full,               // nullary function (constant): full relation
  choice,             // binary function
  intersection,       // binary function
  composition,        // binary function
  transitiveClosure,  // unary function
  converse,           // unary function
  cartesianProduct    // TODO:
};

class Relation {
 public:
  /* Rule of five */
  Relation(const Relation &other);
  Relation(Relation &&other) = default;
  Relation &operator=(const Relation &other);
  Relation &operator=(Relation &&other) = default;
  ~Relation() = default;
  friend void swap(Relation &first, Relation &second) {
    using std::swap;
    swap(first.operation, second.operation);
    swap(first.identifier, second.identifier);
    swap(first.leftOperand, second.leftOperand);
    swap(first.rightOperand, second.rightOperand);
    swap(first.saturated, second.saturated);
    swap(first.saturatedId, second.saturatedId);
  }
  bool operator==(const Relation &other) const;  // compares two relation syntactically

  explicit Relation(const std::string &expression);  // parse constructor
  explicit Relation(const RelationOperation operation,
                    const std::optional<std::string> &identifier = std::nullopt);
  Relation(const RelationOperation operation, Relation &&left);
  Relation(const RelationOperation operation, Relation &&left, Relation &&right);

  RelationOperation operation;
  std::optional<std::string> identifier;   // is set iff operation base
  std::unique_ptr<Relation> leftOperand;   // is set iff operation unary/binary
  std::unique_ptr<Relation> rightOperand;  // is set iff operation binary

  bool saturated = false;  // mark base relation
  bool saturatedId = false;

  std::string toString() const;  // for printing
};
