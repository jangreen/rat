#pragma once
#include "Literal.h"

// Uncomment to use the old worklist implementation (ordered sets).
// #define WORKLIST_ALTERNATIVE
class Worklist;
class Tableau;

class Node {
 private:
#ifndef WORKLIST_ALTERNATIVE
  // Intrusive worklist design for O(1) insertion and removal.
  friend class Worklist;
  Node *nextInWorkList = nullptr;
  Node *prevInWorkList = nullptr;
#endif
 public:
  Node(Node *parent, Literal literal);
  explicit Node(const Node *other) = delete;
  ~Node();
  [[nodiscard]] bool validate() const;
  [[nodiscard]] bool validateRecursive() const;

  Tableau *tableau;
  const Literal literal;
  Node *parentNode = nullptr;
  std::vector<std::unique_ptr<Node>> children;

  // gather information about the prefix of the branch
  // only at leaf nodes
  const EventSet activeEvents;
  const SetOfSets activeEventBasePairs;

  [[nodiscard]] bool isClosed() const;
  [[nodiscard]] bool isLeaf() const;

  // this method assumes that tableau is already reduced
  [[nodiscard]] DNF extractDNF() const;
  void dnfBuilder(DNF &dnf) const;

  void toDotFormat(std::ofstream &output) const;
};

/*
 * The worklist manages the nodes that need to get processed.
 * Importantly, it makes sure the processing order is sound and efficient:
 *  1. Positive equalities
 *1.5(?) Set membership (currently not used/supported)
 *  2. non normal positive literals (i.e., the rest)
 *  3. non normal negative literals
 *  4. remaining
 *
 *  Order for Rules 1. and 2.  are for soundness.
 *  2.: we filter non active literals in appendBranch
 *  Rule 3. is for efficiency in order to close branches as soon as possible.
 */
class Worklist {
 private:
#ifndef WORKLIST_ALTERNATIVE
  // (1)
  std::unique_ptr<Node> posEqualitiesHeadDummy;
  std::unique_ptr<Node> posEqualitiesTailDummy;
  // (3)
  std::unique_ptr<Node> nonNormalNegatedHeadDummy;
  std::unique_ptr<Node> nonNormalNegatedTailDummy;
  // (3)
  std::unique_ptr<Node> nonNormalPositiveHeadDummy;
  std::unique_ptr<Node> nonNormalPositiveTailDummy;
  // (4)
  std::unique_ptr<Node> remainingHeadDummy;
  std::unique_ptr<Node> remainingTailDummy;

  static void connect(Node &left, Node &right);
  static void disconnect(Node &node);
  static void insertAfter(Node &location, Node &node);
  static bool isEmpty(const std::unique_ptr<Node> &head, const std::unique_ptr<Node> &tail);
#else
  struct CompareNodes {
    bool operator()(const Node *left, const Node *right) const;
  };

  std::set<Node *, CompareNodes> queue;
#endif
 public:
  Worklist();
  void push(Node *node);
  Node *pop();
  void erase(Node *node);
  bool contains(const Node *node) const;

  [[nodiscard]] bool isEmpty() const;
  [[nodiscard]] bool validate() const;
};