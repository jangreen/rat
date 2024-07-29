#pragma once

#include <vector>

#include "../basic/Literal.h"

class Tableau;
class Node;
typedef std::vector<Node *> NodeCube;

class Node {
 private:
  // ================== Intrusive Worklist ==================
  friend class Worklist;
  mutable Node *nextInWorkList = nullptr;
  mutable Node *prevInWorkList = nullptr;
  // To generate dummy sentinel nodes
  Node() : tableau(nullptr), literal(BOTTOM) {}

  // ================== Core members ==================
  Tableau *const tableau;
  Node *parentNode = nullptr;
  Literal literal;
  const Node *lastUnrollingParent = nullptr;  // to detect at the world cycles
  std::vector<std::unique_ptr<Node>> children;

  // ================== Cached ==================
  mutable bool _isClosed = false;

  void appendBranchInternalUp(DNF &dnf) const;
  void appendBranchInternalDownDisjunctive(DNF &dnf);
  void appendBranchInternalDownConjunctive(const DNF &dnf);
  void reduceBranchInternalDown(NodeCube &nodeCube);
  void inferModalAtomicUp(CanonicalSet search1, CanonicalSet replace1, CanonicalSet search2,
                          CanonicalSet replace2);
  void inferModalAtomicDown(CanonicalSet search1, CanonicalSet replace1, CanonicalSet search2,
                            CanonicalSet replace2);
  Cube inferModalAtomicNode(CanonicalSet search1, CanonicalSet replace1, CanonicalSet search2,
                            CanonicalSet replace2);
  void inferModalUp();
  void inferModalDown(const Literal &negatedLiteral);
  void inferModalBaseSetUp();
  void inferModalBaseSetDown(const Literal &negatedLiteral);
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
  void setLastUnrollingParent(const Node *node);
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
  std::optional<DNF> applyRule();
  void inferModal();
  void inferModalTop();
  void inferModalBaseSet();
  void inferModalAtomic();

  // ================== Printing ==================
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
   private:
    Node *node;
    int childIndex;

   public:
    using iterator_category = std::input_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = Node *;
    using pointer = value_type *;
    using reference = value_type &;
    struct EndSentinel {};

    explicit ChildIterator(Node *node)
        : node(node), childIndex(static_cast<int>(node->children.size()) - 1) {}

    value_type operator*() const { return node->children.at(childIndex).get(); }
    value_type operator->() const { return node->children.at(childIndex).get(); }
    ChildIterator &operator++() {
      --childIndex;
      return *this;
    }
    bool operator==(const EndSentinel sentinel) const {
      return childIndex < 0 || node->children.empty();
    }

    [[nodiscard]] bool isLast() const { return childIndex == 0; }
  };

  ChildIterator beginSafe() { return ChildIterator(this); }
  ChildIterator::EndSentinel endSafe() { return {}; }
};

inline const Node *Node::transitiveClosureNode = nullptr;
