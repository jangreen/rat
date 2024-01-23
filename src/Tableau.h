#pragma once
#include <fstream>
#include <iostream>
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "Formula.h"

class Tableau {
 public:
  class Node {
   public:
    Node(Tableau *tableau, const Formula &&formula);
    Node(Node *parent, const Formula &&formula);

    Tableau *tableau;
    Formula formula;
    std::unique_ptr<Node> leftNode = nullptr;
    std::unique_ptr<Node> rightNode = nullptr;
    Node *parentNode = nullptr;

    bool isClosed() const;
    bool isLeaf() const;
    bool branchContains(const Formula &formula);
    void appendBranch(const GDNF &formulas);
    bool appendable(const FormulaSet &formulas);
    void appendBranch(const Formula &leftFormula);
    void appendBranch(const Formula &leftFormula, const Formula &rightFormula);
    std::optional<GDNF> applyRule(bool modalRule = false);
    void inferModal();
    void inferModalAtomic();

    // this method assumes that tableau is already reduced
    std::vector<std::vector<Formula>> extractDNF() const;

    void toDotFormat(std::ofstream &output) const;

    struct CompareNodes {
      bool operator()(const Node *left, const Node *right) const;
    };
  };

  Tableau(std::initializer_list<Formula> initalFormulas);
  explicit Tableau(FormulaSet initalFormulas);

  std::unique_ptr<Node> rootNode;
  std::priority_queue<Node *, std::vector<Node *>, Node::CompareNodes> unreducedNodes;

  bool solve(int bound = 1000);  // TODO: remove bound for regular reasoning since it terminates

  // methods for regular reasoning
  std::optional<Formula> applyRuleA();
  GDNF dnf();

  void toDotFormat(std::ofstream &output) const;
  void exportProof(std::string filename) const;

  // helper
  static std::vector<Literal> substitute(Literal &literal, Set &search, Set &replace) {
    int c = 1;
    Literal copy = literal;
    std::vector<Literal> newLiterals;
    while (copy.substitute(search, replace, c) == 0) {
      newLiterals.push_back(copy);
      copy = literal;
      c++;
    }
    return newLiterals;
  }
};
