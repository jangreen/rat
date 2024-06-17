#pragma once
#include <fstream>
#include <iostream>
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "Literal.h"

class Tableau {
 public:
  class Node {
   public:
    Node(Tableau *tableau, const Literal &&literal);
    Node(Node *parent, const Literal &&literal);

    Tableau *tableau;
    Literal literal;
    std::unique_ptr<Node> leftNode = nullptr;
    std::unique_ptr<Node> rightNode = nullptr;
    Node *parentNode = nullptr;

    bool isClosed() const;
    bool isLeaf() const;
    bool branchContains(const Literal &literal);
    void appendBranch(const DNF &cubes);
    bool appendable(const Cube &cube);
    void appendBranch(const Literal &leftLiteral);
    void appendBranch(const Literal &leftLiteral, const Literal &rightLiteral);
    std::optional<DNF> applyRule(bool modalRule = false);
    void inferModal();
    void inferModalTop();
    void inferModalAtomic();

    // this method assumes that tableau is already reduced
    std::vector<std::vector<Literal>> extractDNF() const;

    void toDotFormat(std::ofstream &output) const;

    struct CompareNodes {
      bool operator()(const Node *left, const Node *right) const;
    };
  };

  Tableau(std::initializer_list<Literal> initalLiterals);
  explicit Tableau(Cube initalLiterals);

  std::unique_ptr<Node> rootNode;
  std::priority_queue<Node *, std::vector<Node *>, Node::CompareNodes> unreducedNodes;

  bool solve(int bound = -1);

  // methods for regular reasoning
  std::optional<Literal> applyRuleA();
  DNF dnf();

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
