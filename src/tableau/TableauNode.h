#pragma once

#include <vector>

#include "../basic/Literal.h"

class Tableau;

class Node {
 private:
  // ================== Intrusive Worklist ==================
  friend class Worklist;
  mutable Node *nextInWorkList = nullptr;
  mutable Node *prevInWorkList = nullptr;
  // To generate dummy sentinel nodes
  Node() : literal(BOTTOM) {}

  // ================== Core members ==================
  Tableau *tableau = nullptr;
  Node *parentNode = nullptr;
  std::vector<std::unique_ptr<Node>> children;
  Literal literal;

  const Node *lastUnrollingParent = nullptr;  // to detect at the world cycles

  // ================== Cached ==================
  // gather information about the prefix of the branch
  mutable EventSet activeEvents;
  mutable SetOfSets activeEventBasePairs;
  mutable bool _isClosed = false;


  void appendBranchInternalUp(DNF &dnf) const;
  void appendBranchInternalDown(DNF &dnf);
  void reduceBranchInternalDown(Cube &cube);
  void closeBranch();

  void dnfBuilder(DNF &dnf) const;

 public:
  Node(Tableau *tableau, Literal literal);
  Node(Node *parent, Literal literal);
  explicit Node(const Node *other) = delete;
  ~Node();

  // ================== Accessors ==================
  Tableau *getTableau() const { return tableau; }
  Node *getParentNode() const { return parentNode; }
  const Literal &getLiteral() const { return literal; }
  std::vector<std::unique_ptr<Node>> const &getChildren() const { return children; }
  const Node *getLastUnrollingParent() const { return lastUnrollingParent; }
  void setLastUnrollingParent(const Node *node) { lastUnrollingParent = node; }

  [[nodiscard]] bool isClosed() const { return _isClosed; }
  [[nodiscard]] bool isLeaf() const { return children.empty(); }

  // ================== Node manipulation ==================
  void newChild(std::unique_ptr<Node> child);
  void newChildren(std::vector<std::unique_ptr<Node>> children);
  std::unique_ptr<Node> removeChild(Node *child);
  std::vector<std::unique_ptr<Node>> removeAllChildren() { return std::move(children); }

  // ================== Tree manipulation ==================
  void rename(const Renaming &renaming);
  void appendBranch(const DNF &dnf);
  void appendBranch(const Cube &cube) {
    if (!cube.empty()) {
      appendBranch(DNF{cube});
    }
  }
  void appendBranch(const Literal &literal) { appendBranch(Cube{literal}); }
  std::optional<DNF> applyRule(bool modalRule = false);
  void inferModal();
  void inferModalTop();
  void inferModalAtomic();

  // this method assumes that tableau is already reduced
  [[nodiscard]] DNF extractDNF() const;
  void toDotFormat(std::ofstream &output) const;

  // ================== Debugging ==================
  [[nodiscard]] bool validate() const;
  [[nodiscard]] bool validateRecursive() const;

  static const Node *transitiveClosureNode;
};

inline const Node *Node::transitiveClosureNode = nullptr;
