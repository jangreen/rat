#pragma once
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

enum class Operation {
  none,               // no relation, only label
  base,               // nullary function (constant): base relation
  identity,           // nullary function (constant): identity relation
  empty,              // nullary function (constant): empty relation
  full,               // nullary function (constant): full relation
  choice,             // binary function
  intersection,       // binary function
  composition,        // binary function
  transitiveClosure,  // unary function
  converse            // unary function
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
    swap(first.label, second.label);
    swap(first.negated, second.negated);
    swap(first.saturated, second.saturated);
    swap(first.saturatedId, second.saturatedId);
  }

  explicit Relation(const std::string &expression);  // parse constructor
  explicit Relation(const Operation operation,
                    const std::optional<std::string> &identifier = std::nullopt);
  Relation(const Operation operation, Relation &&left);
  Relation(const Operation operation, Relation &&left, Relation &&right);

  Operation operation;
  std::optional<std::string> identifier;    // is set iff operation base
  std::unique_ptr<Relation> leftOperand;    // is set iff operation unary/binary
  std::unique_ptr<Relation> rightOperand;   // is set iff operation binary
  std::optional<int> label = std::nullopt;  // is set iff labeled term
  bool negated = false;                     // propsitional negation
  bool saturated = false;                   // mark base relation
  bool saturatedId = false;

  bool isNormal() const;                       // true iff all labels are in front of base relations
  std::vector<int> labels() const;             // return all labels of the relation term
  std::vector<int> calculateRenaming() const;  // renaming {2,4,5}: 2->0,4->1,5->2
  void rename(const std::vector<int> &renaming);         // renames given a renaming function
  bool operator==(const Relation &otherRelation) const;  // compares two relation syntactically
  bool operator<(const Relation &otherRelation) const;   // for sorting/hashing
  std::string toString() const;                          // for printing

  static std::unordered_map<std::string, Relation>
      relations;        // id, 0, 1, base relations and defined relations (named relations)
  static int maxLabel;  // to create globally unique labels
};

typedef std::vector<Relation> Clause;
