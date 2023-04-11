#pragma once
#include <fstream>
#include <memory>
#include <queue>
#include <string>
#include <tuple>
#include <vector>

#include "Metastatement.h"
#include "Relation.h"

class Tableau {
 public:
  class Node {
   public:
    Node(Tableau *tableau, const Relation &&relation);
    Node(Tableau *tableau, const Metastatement &&metastatement);

    Tableau *tableau;
    std::optional<Relation> relation = std::nullopt;
    std::optional<Metastatement> metastatement = std::nullopt;
    std::unique_ptr<Node> leftNode = nullptr;
    std::unique_ptr<Node> rightNode = nullptr;
    Node *parentNode = nullptr;
    Node *parentMetastatement = nullptr;  // metastatement chain
    bool closed = false;

    bool isClosed();
    bool isLeaf() const;
    void appendBranches(const Relation &leftRelation, const Relation &rightRelation);
    void appendBranches(const Relation &leftRelation);
    void appendBranches(const Metastatement &metastatement);
    template <ProofRule::Rule rule, typename ConclusionType>
    std::optional<ConclusionType> applyRule();
    bool apply(const std::initializer_list<ProofRule> rules);
    bool applyAnyRule();
    bool applyDNFRule();

    template <typename ClauseType>
    std::vector<ClauseType> extractDNF();

    void toDotFormat(std::ofstream &output) const;

    struct CompareNodes {
      bool operator()(const Node *left, const Node *right) const;
    };
  };

  Tableau(std::initializer_list<Relation> initalRelations);
  explicit Tableau(Clause initalRelations);

  std::unique_ptr<Node> rootNode;
  std::priority_queue<Node *, std::vector<Node *>, Node::CompareNodes> unreducedNodes;

  bool solve(int bound = 30);

  // methods for regular reasoning
  template <typename ClauseType>
  std::vector<ClauseType> DNF();

  bool apply(const std::initializer_list<ProofRule> rules);
  std::optional<Metastatement> applyRuleA();
  void calcReuqest();
  std::tuple<Clause, Clause> extractRequest() const;  // and converse request

  void toDotFormat(std::ofstream &output) const;
  void exportProof(std::string filename) const;
};

// template implementations

template <typename ClauseType>
std::vector<ClauseType> Tableau::DNF() {
  while (!unreducedNodes.empty()) {
    auto currentNode = unreducedNodes.top();
    unreducedNodes.pop();
    if (currentNode->metastatement) {
      // only equality meatstatement possible
      currentNode->applyRule<ProofRule::propagation, bool>();
    } else {
      currentNode->applyDNFRule();
    }
  }

  exportProof("dnfcalc");  // TODO: remove

  return rootNode->extractDNF<ClauseType>();
};

template <typename ClauseType>
std::vector<ClauseType> Tableau::Node::extractDNF() {
  if (isClosed()) {
    return {};
  }
  if (isLeaf()) {
    if (relation && relation->isNormal()) {
      return {{*relation}};
    }
    if constexpr (std::is_same_v<ClauseType, ExtendedClause>) {
      if (metastatement && metastatement->type == MetastatementType::labelEquality) {
        return {{*metastatement}};
      }
    }
    return {};
  } else {
    std::vector<ClauseType> result;
    if (leftNode != nullptr) {
      auto left = leftNode->extractDNF<ClauseType>();
      result.insert(result.end(), left.begin(), left.end());
    }
    if (rightNode != nullptr) {
      auto right = rightNode->extractDNF<ClauseType>();
      result.insert(result.end(), right.begin(), right.end());
    }
    if (relation && relation->isNormal()) {
      if (result.empty()) {
        result.push_back({});
      }
      for (auto &clause : result) {
        clause.push_back(*relation);
      }
    } else if constexpr (std::is_same_v<ClauseType, ExtendedClause>) {
      if (metastatement && metastatement->type == MetastatementType::labelEquality) {
        if (result.empty()) {
          result.push_back({});
        }
        for (auto &clause : result) {
          clause.push_back(*metastatement);
        }
      }
    }
    return result;
  }
}
