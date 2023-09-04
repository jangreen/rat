#pragma once
#include <fstream>
#include <memory>
#include <queue>
#include <string>
#include <tuple>
#include <vector>

#include "Formula.h"
#include "Relation.h"

class Tableau {
 public:
  class Node {
   public:
    Node(Tableau *tableau, const Formula &&relation);

    Tableau *tableau;
    std::optional<Formula> formula = std::nullopt;
    std::unique_ptr<Node> leftNode = nullptr;
    std::unique_ptr<Node> rightNode = nullptr;
    Node *parentNode = nullptr;
    bool closed = false;

    bool isClosed();
    bool isLeaf() const;
    void appendBranches(const Formula &leftRelation, const Formula &rightRelation);
    void appendBranches(const Formula &leftRelation);
    template <ProofRule::Rule rule, typename ConclusionType>
    std::optional<ConclusionType> applyRule();
    bool apply(const std::initializer_list<ProofRule> rules);
    bool applyAnyRule();
    bool applyDNFRule();

    DNF extractDNF();

    void toDotFormat(std::ofstream &output) const;

    struct CompareNodes {
      bool operator()(const Node *left, const Node *right) const;
    };
  };

  Tableau(std::initializer_list<Formula> initalFormulas);
  explicit Tableau(FormulaSet initalFormulas);

  std::unique_ptr<Node> rootNode;
  std::priority_queue<Node *, std::vector<Node *>, Node::CompareNodes> unreducedNodes;

  bool solve(int bound = 30);

  // methods for regular reasoning
  DNF calcDNF();

  bool apply(const std::initializer_list<ProofRule> rules);
  std::optional<Metastatement> applyRuleA();
  void calcReuqest();
  std::tuple<ExtendedClause, Clause> extractRequest() const;  // and converse request

  void toDotFormat(std::ofstream &output) const;
  void exportProof(std::string filename) const;
};
