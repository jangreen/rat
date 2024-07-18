#include "Tableau.h"

//------------------------------------------------------------------------------------------------
// Private methods

void Worklist::connect(Node &left, Node &right) {
  left.nextInWorkList = &right;
  right.prevInWorkList = &left;
}

void Worklist::disconnect(const Node &node) {
  assert(node.nextInWorkList != nullptr);
  assert(node.prevInWorkList != nullptr);
  connect(*node.prevInWorkList, *node.nextInWorkList);
  node.prevInWorkList = nullptr;
  node.nextInWorkList = nullptr;
}

void Worklist::insertAfter(Node &location, Node &node) {
  assert(location.nextInWorkList != nullptr);
  Node *next = location.nextInWorkList;
  connect(location, node);
  connect(node, *next);
}

bool Worklist::isEmpty(const std::unique_ptr<Node> &head, const std::unique_ptr<Node> &tail) {
  return head->nextInWorkList == tail.get();
}

//------------------------------------------------------------------------------------------------
// Public methods

Worklist::Worklist() {
  // Setup dummy sentinel nodes for the doubly linked lists
  posEqualitiesHeadDummy = std::make_unique<Node>(nullptr, BOTTOM);
  posEqualitiesTailDummy = std::make_unique<Node>(nullptr, BOTTOM);
  connect(*posEqualitiesHeadDummy, *posEqualitiesTailDummy);

  nonNormalNegatedHeadDummy = std::make_unique<Node>(nullptr, BOTTOM);
  nonNormalNegatedTailDummy = std::make_unique<Node>(nullptr, BOTTOM);
  connect(*nonNormalNegatedHeadDummy, *nonNormalNegatedTailDummy);

  nonNormalPositiveHeadDummy = std::make_unique<Node>(nullptr, BOTTOM);
  nonNormalPositiveTailDummy = std::make_unique<Node>(nullptr, BOTTOM);
  connect(*nonNormalPositiveHeadDummy, *nonNormalPositiveTailDummy);

  remainingHeadDummy = std::make_unique<Node>(nullptr, BOTTOM);
  remainingTailDummy = std::make_unique<Node>(nullptr, BOTTOM);
  connect(*remainingHeadDummy, *remainingTailDummy);
}

void Worklist::push(Node *node) {
  assert(node->prevInWorkList == nullptr);
  assert(node->nextInWorkList == nullptr);

  const PredicateOperation &op = node->literal.operation;
  const bool negated = node->literal.negated;
  const bool isNormal = node->literal.isNormal();
  if (op == PredicateOperation::constant) {
    // TODO: Make sure that this is correct
    assert(!negated);  // BOTTOM is not handled currently
    return;            // TOP doesn't ever need to get processed.
  }

  Node *insertionPoint;
  if (!isNormal) {
    insertionPoint = negated ? nonNormalNegatedHeadDummy.get() : nonNormalPositiveHeadDummy.get();
  } else if (op == PredicateOperation::equality) {
    insertionPoint = posEqualitiesHeadDummy.get();
  } else {
    // Heuristic: It seems to be better to put leafs towards the end, making sure that
    // non-leafs get priority.
    if (node->isLeaf()) {
      insertionPoint = remainingTailDummy->prevInWorkList;
    } else {
      insertionPoint = remainingHeadDummy.get();
    }
  }

  insertAfter(*insertionPoint, *node);
}

void Worklist::erase(const Node *node) {
  if (node->prevInWorkList == nullptr || node->nextInWorkList == nullptr) {
    // The node is a dummy that cannot be erased.
    return;
  }
  disconnect(*node);
}

bool Worklist::contains(const Node *node) const {
  // This assumes there exists only a single worklist
  return node->prevInWorkList != nullptr && node->nextInWorkList != nullptr;
}

Node *Worklist::pop() {
  Node *next;
  if (!isEmpty(posEqualitiesHeadDummy, posEqualitiesTailDummy)) {
    next = posEqualitiesHeadDummy->nextInWorkList;
  } else if (!isEmpty(nonNormalPositiveHeadDummy, nonNormalPositiveTailDummy)) {
    next = nonNormalPositiveHeadDummy->nextInWorkList;
  } else if (!isEmpty(nonNormalNegatedHeadDummy, nonNormalNegatedTailDummy)) {
    next = nonNormalNegatedHeadDummy->nextInWorkList;
  } else if (!isEmpty(remainingHeadDummy, remainingTailDummy)) {
    next = remainingHeadDummy->nextInWorkList;
  } else {
    next = nullptr;
  }
  assert(next != nullptr);
  disconnect(*next);
  return next;
}

bool Worklist::isEmpty() const {
  return isEmpty(remainingHeadDummy, remainingTailDummy) &&
         isEmpty(nonNormalNegatedHeadDummy, nonNormalNegatedTailDummy) &&
         isEmpty(nonNormalPositiveHeadDummy, nonNormalPositiveTailDummy) &&
         isEmpty(posEqualitiesHeadDummy, posEqualitiesTailDummy);
}

bool Worklist::validate() const {
  // TODO: Implement iterator and use it here
  for (const Node *cur = posEqualitiesHeadDummy->nextInWorkList;
       cur != posEqualitiesTailDummy.get(); cur = cur->nextInWorkList) {
    if (!cur->validate()) return false;
  }
  for (const Node *cur = nonNormalNegatedHeadDummy->nextInWorkList;
       cur != nonNormalNegatedTailDummy.get(); cur = cur->nextInWorkList) {
    if (!cur->validate()) return false;
  }
  for (const Node *cur = nonNormalPositiveHeadDummy->nextInWorkList;
       cur != nonNormalPositiveTailDummy.get(); cur = cur->nextInWorkList) {
    if (!cur->validate()) return false;
  }
  for (const Node *cur = remainingHeadDummy->nextInWorkList; cur != remainingTailDummy.get();
       cur = cur->nextInWorkList) {
    if (!cur->validate()) return false;
  }

  return true;
}
