#pragma once
#include <fstream>
#include <memory>
#include <queue>
#include <string>
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
    template <ProofRule::Rule rule>
    bool applyRule();
    bool applyDNFRule();

    void toDotFormat(std::ofstream &output) const;

    struct CompareNodes {
      bool operator()(const Node *left, const Node *right) const;
    };
  };

  Tableau(std::initializer_list<Relation> initalRelations);
  explicit Tableau(Clause initalRelations);

  std::unique_ptr<Node> rootNode;
  std::priority_queue<Node *, std::vector<Node *>, Node::CompareNodes> unreducedNodes;

  bool applyRule(Node *node);
  bool solve(int bound = 30);

  // methods for regular reasoning
  template <typename ClauseType>
  std::vector<ClauseType> DNF() {
    while (!unreducedNodes.empty()) {
      auto currentNode = unreducedNodes.top();
      unreducedNodes.pop();
      // TODO: remove exportProof("dnfcalc");
      if (currentNode->metastatement) {
        // only equality meatstatement possible
        currentNode->applyRule<ProofRule::propagation>();
      } else {
        currentNode->applyDNFRule();
      }
    }

    exportProof("dnfcalc");

    return extractDNF<ClauseType>(rootNode.get());
  }
  bool applyModalRule();
  Clause calcReuqest();

  void toDotFormat(std::ofstream &output) const;
  void exportProof(std::string filename) const;
};

// helper
template <typename ClauseType>
std::vector<ClauseType> extractDNF(Tableau::Node *node) {
  if (node == nullptr || node->isClosed()) {
    return {};
  }
  if (node->isLeaf()) {
    if (node->relation && node->relation->isNormal()) {
      return {{*node->relation}};
    }
    if constexpr (std::is_same_v<ClauseType, ExtendedClause>) {
      if (node->metastatement && node->metastatement->type == MetastatementType::labelEquality) {
        return {{*node->metastatement}};
      }
    }
    return {};
  } else {
    auto left = extractDNF<ClauseType>(node->leftNode.get());
    auto right = extractDNF<ClauseType>(node->rightNode.get());
    left.insert(left.end(), right.begin(), right.end());
    if (node->relation && node->relation->isNormal()) {
      if (left.empty()) {
        left.push_back({});
      }
      for (auto &clause : left) {
        clause.push_back(*node->relation);
      }
    } else if constexpr (std::is_same_v<ClauseType, ExtendedClause>) {
      if (node->metastatement && node->metastatement->type == MetastatementType::labelEquality) {
        if (left.empty()) {
          left.push_back({});
        }
        for (auto &clause : left) {
          clause.push_back(*node->metastatement);
        }
      }
    }
    return left;
  }
}