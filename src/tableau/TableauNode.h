#pragma once

#include <vector>

#include "../basic/Literal.h"

class Tableau;
class Node;
typedef std::vector<Node *> NodeCube;

class Node {
  // ================== Intrusive Worklist ==================
  friend class Worklist;
  mutable Node *nextInWorkList = nullptr;
  mutable Node *prevInWorkList = nullptr;
  // To generate dummy sentinel nodes
  Node() : tableau(nullptr), literal(Literal::BOTTOM()) {}

  // ================== Core members ==================
  Tableau *const tableau;
  Node *parentNode = nullptr;
  std::vector<std::unique_ptr<Node>> children;
  Literal literal;

  const Node *lastUnrollingParent = nullptr;  // to detect at the world cycles

  // ================== Cached ==================
  // gather information about the prefix of the branch
  mutable bool _isClosed = false;
  mutable std::optional<boost::container::flat_set<SetOfSets>> _activeEventBasePairs;

  void appendBranchInternalUp(DNF &dnf) const;
  void appendBranchInternalDownDisjunctive(DNF &dnf);
  void appendBranchInternalDownConjunctive(const DNF &dnf);
  void appendBranchInternal(DNF &dnf);

  void reduceBranchInternalDown(NodeCube &nodeCube);
  void reduceBranchInternalDown(Cube &cube);

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

  void closeBranch();

  void dnfBuilder(DNF &dnf) const;

 public:
  Node(Tableau *tableau, Literal literal);
  Node(Node *parent, Literal literal);
  explicit Node(const Node *other) = delete;
  ~Node();

  Cube equalities;  // used to track renameBranches

  // ================== Accessors ==================
  [[nodiscard]] Tableau *getTableau() const;
  [[nodiscard]] Node *getParentNode() const;
  [[nodiscard]] const Literal &getLiteral() const;
  [[nodiscard]] std::vector<std::unique_ptr<Node>> const &getChildren() const;
  [[nodiscard]] const Node *getLastUnrollingParent() const;
  void setLastUnrollingParent(const Node *newLastUnrollingParent);
  [[nodiscard]] bool isClosed() const;
  [[nodiscard]] bool isLeaf() const;
  size_t size() const;

  // ================== Node manipulation ==================
  void attachChild(std::unique_ptr<Node> child);
  void attachChildren(std::vector<std::unique_ptr<Node>> newChildren);
  [[nodiscard]] std::unique_ptr<Node> detachChild(Node *child);
  [[nodiscard]] std::vector<std::unique_ptr<Node>> detachAllChildren();
  [[nodiscard]] std::unique_ptr<Node> detachFromParent();

  void rename(const Renaming &renaming);

  // ================== Tree manipulation ==================
  void appendBranch(const DNF &dnf);
  void appendBranch(const Cube &cube);
  void appendBranch(const Literal &literal);

  std::optional<DNF> applyRule();
  void inferModal();
  void inferModalTop();
  void inferModalBaseSet();
  void inferModalAtomic();
  void removeTrueLiterals(boost::container::flat_set<SetOfSets> &activePairCubes);
  void computeActivePairs(SetOfSets &prefixActivePairs) const;

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
