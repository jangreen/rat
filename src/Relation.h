#pragma once
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ProofRule.h"

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

  explicit Relation(const std::string &expression);  // parse constructor
  explicit Relation(const RelationOperation operation,
                    const std::optional<std::string> &identifier = std::nullopt);
  Relation(const RelationOperation operation, Relation &&left);
  Relation(const RelationOperation operation, Relation &&left, Relation &&right);

  RelationOperation operation;
  std::optional<std::string> identifier;    // is set iff operation base
  std::unique_ptr<Relation> leftOperand;    // is set iff operation unary/binary
  std::unique_ptr<Relation> rightOperand;   // is set iff operation binary
  std::optional<int> label = std::nullopt;  // is set iff labeled term
  bool saturated = false;                   // mark base relation
  bool saturatedId = false;

  bool isNormal() const;                       // true iff all labels are in front of base relations
  std::vector<int> labels() const;             // return all labels of the relation term
  std::vector<int> calculateRenaming() const;  // renaming {2,4,5}: 2->0,4->1,5->2
  void rename(const std::vector<int> &renaming);  // renames given a renaming function
  void inverseRename(const std::vector<int> &renaming);
  bool operator==(const Relation &otherRelation) const;  // compares two relation syntactically
  bool operator<(const Relation &otherRelation) const;   // for sorting/hashing
  std::string toString() const;                          // for printing

  template <ProofRule::Rule rule, typename ConclusionType>
  std::optional<ConclusionType> applyRule();
  // this function tries to apply a rule deep inside
  // returns  - false if rule is not applicable
  //          - result otherwise
  template <ProofRule::Rule rule, typename ConclusionType>
  std::optional<ConclusionType> applyRuleDeep() {
    auto baseCase = applyRule<rule, ConclusionType>();
    if (baseCase) {
      return *baseCase;
    }
    // case: intersection or composition (only cases for labeled terms that can happen)
    if (operation == RelationOperation::composition ||
        operation == RelationOperation::intersection) {
      auto leftConclusion = leftOperand->applyRuleDeep<rule, ConclusionType>();
      if (leftConclusion) {
        return substituteLeft<ConclusionType>(std::move(*leftConclusion));
      }
      // case: intersection
      if (operation == RelationOperation::intersection) {
        auto rightConclusion = rightOperand->applyRuleDeep<rule, ConclusionType>();
        if (rightConclusion) {
          return substituteRight<ConclusionType>(std::move(*rightConclusion));
        }
      }
    }
    return std::nullopt;
  }
  template <typename ConclusionType>
  ConclusionType substituteLeft(ConclusionType &&newLeft);
  template <typename ConclusionType>
  ConclusionType substituteRight(ConclusionType &&newRight);

  static std::unordered_map<std::string, Relation> relations;  // defined relations
  static int maxLabel;                                         // to create globally unique labels
};
