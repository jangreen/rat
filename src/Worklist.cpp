#include "Tableau.h"

#ifndef WORKLIST_ALTERNATIVE

void Tableau::Worklist::connect(Node &left, Node &right) {
  left.nextInWorkList = &right;
  right.prevInWorkList = &left;
}

void Tableau::Worklist::disconnect(Node &node) {
  assert(node.nextInWorkList != nullptr);
  assert(node.prevInWorkList != nullptr);
  connect(*node.prevInWorkList, *node.nextInWorkList);
  node.prevInWorkList = nullptr;
  node.nextInWorkList = nullptr;
}

void Tableau::Worklist::insertAfter(Node &location, Node &node) {
  assert(location.nextInWorkList != nullptr);
  Node *next = location.nextInWorkList;
  connect(location, node);
  connect(node, *next);
}

bool Tableau::Worklist::isEmpty(const std::unique_ptr<Node> &head, const std::unique_ptr<Node> &tail) {
  return head->nextInWorkList == tail.get();
}

Tableau::Worklist::Worklist() {
  // Setup dummy sentinel nodes for the doubly linked lists
  posEqualitiesHeadDummy = std::make_unique<Node>(nullptr, BOTTOM);
  posEqualitiesTailDummy = std::make_unique<Node>(nullptr, BOTTOM);
  connect(*posEqualitiesHeadDummy, *posEqualitiesTailDummy);

  posTopSetHeadDummy = std::make_unique<Node>(nullptr, BOTTOM);
  posTopSetTailDummy = std::make_unique<Node>(nullptr, BOTTOM);
  connect(*posTopSetHeadDummy, *posTopSetTailDummy);

  negativesHeadDummy = std::make_unique<Node>(nullptr, BOTTOM);
  negativesTailDummy = std::make_unique<Node>(nullptr, BOTTOM);
  connect(*negativesHeadDummy, *negativesTailDummy);

  positivesHeadDummy = std::make_unique<Node>(nullptr, BOTTOM);
  positivesTailDummy = std::make_unique<Node>(nullptr, BOTTOM);
  connect(*positivesHeadDummy, *positivesTailDummy);
}

void Tableau::Worklist::push(Node *node) {
  assert(node->prevInWorkList == nullptr);
  assert(node->nextInWorkList == nullptr);

  const PredicateOperation &op = node->literal.operation;
  const bool negated = node->literal.negated;
  if (op == PredicateOperation::constant) {
    // TODO: Make sure that this is correct
    assert(!negated); // BOTTOM is not handled currently
    return; // TOP doesn't ever need to get processed.
  }

  Node *insertionPoint;
  if (negated) {
    insertionPoint = negativesHeadDummy.get();
  } else if (op == PredicateOperation::equality) {
    insertionPoint = posEqualitiesHeadDummy.get();
  } else if (node->literal.hasTopSet()) {
    insertionPoint = posTopSetHeadDummy.get();
  } else {
    // Heuristic: It seems to be better to put leafs towards the end, making sure that
    //  non-leafs get priority.
    if (node->isLeaf()) {
      insertionPoint = positivesTailDummy->prevInWorkList;
    } else {
      insertionPoint = positivesHeadDummy.get();
    }
  }

  insertAfter(*insertionPoint, *node);
}

void Tableau::Worklist::erase(Node *node) {
  if (node->prevInWorkList == nullptr || node->nextInWorkList == nullptr) {
    // The node is a dummy that cannot be erased.
    // TODO: Double check edge case: Worklist gets destroyed, causing dummies to get deleted,
    //  triggering the Node destructor which calls erase here.
    return;
  }
  disconnect(*node);
}

bool Tableau::Worklist::contains(const Node *node) const {
  // This assumes there exists only a single worklist
  return node->prevInWorkList != nullptr && node->nextInWorkList != nullptr;
}

Tableau::Node *Tableau::Worklist::pop() {
  Node *next;
  if (!isEmpty(posEqualitiesHeadDummy, posEqualitiesTailDummy)) {
    next = posEqualitiesHeadDummy->nextInWorkList;
  } else if (!isEmpty(posTopSetHeadDummy, posTopSetTailDummy)) {
    next = posTopSetHeadDummy->nextInWorkList;
  } else if (!isEmpty(positivesHeadDummy, positivesTailDummy)) {
    next = positivesHeadDummy->nextInWorkList;
  } else if (!isEmpty(negativesHeadDummy, negativesTailDummy)) {
    next = negativesHeadDummy->nextInWorkList;
  } else {
    next = nullptr;
  }
  assert(next != nullptr);
  disconnect(*next);
  return next;
}

bool Tableau::Worklist::isEmpty() const {
  return isEmpty(positivesHeadDummy, positivesTailDummy) && isEmpty(negativesHeadDummy, negativesTailDummy)
  && isEmpty(posTopSetHeadDummy, posTopSetTailDummy) && isEmpty(posEqualitiesHeadDummy, posEqualitiesTailDummy);
}

bool Tableau::Worklist::validate() const {
  // TODO: Implement iterator and use it here
  for (const Node *cur = posEqualitiesHeadDummy->nextInWorkList; cur != posEqualitiesTailDummy.get(); cur = cur->nextInWorkList) {
    if (!cur->validate())
      return false;
  }
  for (const Node *cur = posTopSetHeadDummy->nextInWorkList; cur != posTopSetTailDummy.get(); cur = cur->nextInWorkList) {
    if (!cur->validate())
      return false;
  }
  for (const Node *cur = negativesHeadDummy->nextInWorkList; cur != negativesTailDummy.get(); cur = cur->nextInWorkList) {
    if (!cur->validate())
      return false;
  }
  for (const Node *cur = positivesHeadDummy->nextInWorkList; cur != positivesTailDummy.get(); cur = cur->nextInWorkList) {
    if (!cur->validate())
      return false;
  }

  return true;
}




#else
// --------------------------------------------------------------
// Alternative worklist implementation

bool Tableau::Worklist::CompareNodes::operator()(const Node *left, const Node *right) const {
  if ((left->literal.operation == PredicateOperation::equality || right->literal.operation == PredicateOperation::equality)
        && left->literal.operation != right->literal.operation) {
    return left->literal.operation == PredicateOperation::equality;
  }

  if (left->literal.hasTopSet() != right->literal.hasTopSet()) {
    return left->literal.hasTopSet();
  }

  // Compare nodes by literals.
  const auto litCmp = left->literal <=> right->literal;
  if (litCmp == 0) {
    // ensure that multiple nodes with same literal are totally ordered (but non-deterministic)
    return left < right;
  }
  return litCmp < 0;
}

Tableau::Worklist::Worklist() = default;

void Tableau::Worklist::push(Node *node) {
  queue.insert(node);
}

void Tableau::Worklist::erase(Node *node) { queue.erase(node); }

bool Tableau::Worklist::contains(Node *node) const {
  return std::ranges::any_of(queue, [&](const auto &n) { return n == node; });
}

Tableau::Node *Tableau::Worklist::pop() {
  return queue.extract(queue.begin()).value();
}

bool Tableau::Worklist::isEmpty() const {
  return queue.empty();
}

bool Tableau::Worklist::validate() const {
  return std::ranges::all_of(queue, &Node::validate);
}

#endif

