
#pragma once
#include "TableauNode.h"

// ============================================================================
// ================================= Worklist =================================
// ============================================================================

/*
 * The worklist manages the nodes that need to get processed in a tableau.
 * Importantly, it makes sure the processing order is sound and efficient:
 *  1. Positive equalities
 *1.5(?) Set membership (currently not used/supported)
 *  2. non-normal positive literals
 *  3. non-normal negative literals
 *  4. remaining
 *
 *  Order for Rules 1. and 2.  are for soundness.
 *  2.: we filter non active literals in appendBranch
 *  Rule 3. is for efficiency in order to close branches as soon as possible.
 *
 *  Implementation note: The worklist is implemented as an intrusive doubly-linked list.
 */
class Worklist {
 private:
  // (1)
  std::unique_ptr<Node> posEqualitiesHeadDummy;
  std::unique_ptr<Node> posEqualitiesTailDummy;
  // (3)
  std::unique_ptr<Node> nonNormalNegatedHeadDummy;
  std::unique_ptr<Node> nonNormalNegatedTailDummy;
  // (3)
  std::unique_ptr<Node> positiveHeadDummy;
  std::unique_ptr<Node> positiveTailDummy;
  // (4)
  std::unique_ptr<Node> remainingHeadDummy;
  std::unique_ptr<Node> remainingTailDummy;

  static void connect(Node &left, Node &right);
  static void disconnect(const Node &node);
  static void insertAfter(Node &location, Node &node);
  static bool isEmpty(const std::unique_ptr<Node> &head, const std::unique_ptr<Node> &tail);

 public:
  Worklist();
  void push(Node *node);
  Node *pop();
  [[nodiscard]] Node *top() const;
  void erase(const Node *node);
  bool contains(const Node *node) const;

  [[nodiscard]] bool isEmpty() const;
  [[nodiscard]] bool validate() const;
};