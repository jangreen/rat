#pragma once
#include <fstream>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "Literal.h"

class Tableau {
 public:
  class Node {
   public:
    Node(Node *parent, Literal literal);
    Node(const Node *other) = delete;
    ~Node();
    bool validate() const;
    bool validateRecursive() const;

    Tableau *tableau;
    const Literal literal;
    std::vector<std::unique_ptr<Node>> children;
    Node *parentNode = nullptr;

    [[nodiscard]] bool isClosed() const;
    [[nodiscard]] bool isLeaf() const;
    void appendBranch(const DNF &dnf);
    void appendBranch(const Cube &cube);
    void appendBranch(const Literal &literal);
    std::optional<DNF> applyRule(bool modalRule = false);
    void inferModal();
    void inferModalTop();
    void inferModalAtomic();
    void replaceNegatedTopOnBranch(std::vector<int> labels);

    // this method assumes that tableau is already reduced
    [[nodiscard]] DNF extractDNF() const;
    void dnfBuilder(DNF &dnf) const;

    void toDotFormat(std::ofstream &output) const;

    struct CompareNodes {
      bool operator()(const Node *left, const Node *right) const;
    };

   private:
    void appendBranchInternalUp(DNF &dnf) const;
    void appendBranchInternalDown(DNF &dnf);
    void closeBranch();
    void getNodesBehind(std::set<Node *, CompareNodes> &nodes);
  };

  class NodeQueue {
    typedef std::set<Node *>::iterator iterator;
    typedef std::set<Node *>::const_iterator const_iterator;

   private:
    std::set<Node *, Node::CompareNodes> queue;

   public:
    void push(Node *node);
    void erase(Node *node);
    Node *pop();
    [[nodiscard]] bool isEmpty() const;

    void removeIf(const std::function<bool(Node *)> &predicate);
    bool validate() const;

    const_iterator cbegin() const { return queue.cbegin(); }
    const_iterator cend() const { return queue.cend(); }
  };

  explicit Tableau(const Cube &initialLiterals);
  bool validate() const;

  std::unique_ptr<Node> rootNode;
  NodeQueue unreducedNodes;

  bool solve(int bound = -1);
  void removeNode(Node *node);
  void renameBranch(Node *leaf, int from, int to);

  // methods for regular reasoning
  bool applyRuleA();
  DNF dnf();

  void toDotFormat(std::ofstream &output) const;
  void exportProof(const std::string &filename) const;
  void exportDebug(const std::string &filename) const;

  // helper
  static Cube substitute(const Literal &literal, CanonicalSet search, CanonicalSet replace) {
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
