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
  [[nodiscard]] Tableau *getTableau() const { return tableau; }
  [[nodiscard]] Node *getParentNode() const { return parentNode; }
  [[nodiscard]] const Literal &getLiteral() const { return literal; }
  [[nodiscard]] std::vector<std::unique_ptr<Node>> const &getChildren() const { return children; }
  [[nodiscard]] const Node *getLastUnrollingParent() const { return lastUnrollingParent; }
  void setLastUnrollingParent(const Node *node) { lastUnrollingParent = node; }

  [[nodiscard]] bool isClosed() const { return _isClosed; }
  [[nodiscard]] bool isLeaf() const { return children.empty(); }

  // ================== Node manipulation ==================
  void attachChild(std::unique_ptr<Node> child);
  void attachChildren(std::vector<std::unique_ptr<Node>> newChildren);
  [[nodiscard]] std::unique_ptr<Node> detachChild(Node *child);
  [[nodiscard]] std::vector<std::unique_ptr<Node>> detachAllChildren();

  [[nodiscard]] std::unique_ptr<Node> detachFromParent() { return parentNode->detachChild(this); }

  void rename(const Renaming &renaming);

  // ================== Tree manipulation ==================
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

  // ================== Extraction ==================
  // this method assumes that tableau is already reduced
  [[nodiscard]] DNF extractDNF() const;
  void toDotFormat(std::ofstream &output) const;

  // ================== Debugging ==================
  [[nodiscard]] bool validate() const;
  [[nodiscard]] bool validateRecursive() const;

  static const Node *transitiveClosureNode;

  // ================== Safe iteration ==================

  /*
   * This iterator tries to minimize problems when deleting children while iterating.
   */
  struct ChildIterator {
  private :
    Node *node;
    int childIndex;

  public:
    using iterator_category = std::input_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = Node*;
    using pointer = value_type*;
    using reference = value_type&;
    struct EndSentinel {};

    explicit ChildIterator(Node* node) : node(node), childIndex(static_cast<int>(node->children.size() - 1)) {}

    value_type operator*() const { return node->children.at(childIndex).get(); }
    value_type operator->() const { return node->children.at(childIndex).get(); }
    ChildIterator& operator++() {--childIndex; return *this;}
    bool operator==(const EndSentinel sentinel) const {
      return childIndex < 0 || node->children.empty();
    }
  };

  ChildIterator beginSafe() { return ChildIterator(this); }
  ChildIterator::EndSentinel endSafe() { return {};}

};

inline const Node *Node::transitiveClosureNode = nullptr;
