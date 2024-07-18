#pragma once

#include <vector>

#include "../basic/Literal.h"

class Tableau;

class Node {
 private:
  // Intrusive worklist design for O(1) insertion and removal.
  friend class Worklist;
  mutable Node *nextInWorkList = nullptr;
  mutable Node *prevInWorkList = nullptr;

  // gather information about the prefix of the branch
  // only at leaf nodes
  EventSet activeEvents;
  SetOfSets activeEventBasePairs;
  mutable bool _isClosed;

  const Node *lastUnrollingParent = nullptr;  // to detect at the world cycles

  Literal literal;
  Node *parentNode = nullptr;
  std::vector<std::unique_ptr<Node>> children;

  void appendBranchInternalUp(DNF &dnf) const;
  void appendBranchInternalDown(DNF &dnf);
  void reduceBranchInternalDown(Cube &cube);
  void closeBranch();

 public:
  Node(Node *parent, Literal literal);
  explicit Node(const Node *other) = delete;
  ~Node();
  [[nodiscard]] bool validate() const;
  [[nodiscard]] bool validateRecursive() const;

  Tableau *tableau;  // TODO: use getter

  Node *getParentNode() const { return parentNode; }
  const Literal &getLiteral() const { return literal; }
  std::vector<std::unique_ptr<Node>> const &getChildren() const { return children; }
  void newChild(std::unique_ptr<Node> child);
  void newChildren(std::vector<std::unique_ptr<Node>> children);
  std::unique_ptr<Node> removeChild(Node *child);
  std::vector<std::unique_ptr<Node>> removeAllChildren() { return std::move(children); }
  const Node *getLastUnrollingParent() const { return lastUnrollingParent; }
  void setLastUnrollingParent(const Node *node) { lastUnrollingParent = node; }

  [[nodiscard]] bool isClosed() const { return _isClosed; }
  [[nodiscard]] bool isLeaf() const;
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
  void dnfBuilder(DNF &dnf) const;

  void toDotFormat(std::ofstream &output) const;

  static const Node *transitiveClosureNode;
};

inline const Node *Node::transitiveClosureNode = nullptr;
