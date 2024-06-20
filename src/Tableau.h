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

    [[nodiscard]] bool isClosed() const;
    [[nodiscard]] bool isLeaf() const;
    bool branchContains(const Literal &lit);
    void appendBranch(const DNF &dnf);
    bool appendable(const Cube &cube);
    void appendBranch(const Literal &leftLiteral);
    void appendBranch(const Literal &leftLiteral, const Literal &rightLiteral);
    std::optional<DNF> applyRule(bool modalRule = false);
    void inferModal();
    void inferModalTop();
    void inferModalAtomic();

    // this method assumes that tableau is already reduced
    [[nodiscard]] DNF extractDNF() const;
    void dnfBuilder(DNF &dnf) const;

    void toDotFormat(std::ofstream &output) const;
  };

  Tableau(std::initializer_list<Literal> initialLiterals); // FIXME: Unused???
  explicit Tableau(const Cube &initialLiterals);

  std::unique_ptr<Node> rootNode;
  std::queue<Node *> unreducedNodes;

  bool solve(int bound = -1);

  // methods for regular reasoning
  std::optional<Literal> applyRuleA();
  DNF dnf();

  void toDotFormat(std::ofstream &output) const;
  void exportProof(const std::string& filename) const;

  // helper
  static Cube substitute(Literal &literal, CanonicalSet search, CanonicalSet replace) {
    int c = 1;
    Literal copy = literal;
    Cube newLiterals;
    while (copy.substitute(search, replace, c)) {
      newLiterals.push_back(copy);
      copy = literal;
      c++;
    }
    return newLiterals;
  }
};
