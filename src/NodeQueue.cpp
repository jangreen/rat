#include "Tableau.h"

void Tableau::NodeQueue::push(Node *node) {
  // TODO: remove: std::cout << "PUSH: " << node->literal.toString() << std::endl;
  queue.insert(node);
}

void Tableau::NodeQueue::erase(Node *node) { queue.erase(node); }

Tableau::Node *Tableau::NodeQueue::pop() {
  // TODO: remove: std::cout << "POP: " << (*queue.begin())->literal.toString() << std::endl;

  assert(!queue.empty());
  return queue.extract(queue.begin()).value();
}
bool Tableau::NodeQueue::isEmpty() const { return queue.empty(); }

void Tableau::NodeQueue::removeIf(const std::function<bool(Node *)> &predicate) {
  // TODO: why does this not work? fails if you have two nodes with
  // std::set<Node *, Tableau::Node::CompareNodes> filteredUnreducedNodes{};
  // std::ranges::set_difference(
  //     tableau->unreducedNodes, uselessNodes,
  //     std::inserter(filteredUnreducedNodes, filteredUnreducedNodes.begin()));
  // swap(tableau->unreducedNodes, filteredUnreducedNodes);
  std::erase_if(queue, predicate);
}

bool Tableau::NodeQueue::validate() const {
  return std::all_of(cbegin(), cend(),
                     [](const auto unreducedNode) { return unreducedNode->validate(); });
}