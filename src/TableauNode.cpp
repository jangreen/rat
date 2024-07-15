#include "TableauNode.h"

#include <iostream>

#include "Rules.h"
#include "utility.h"

namespace {
// ---------------------- Anonymous helper functions ----------------------

// initialization helper
EventSet calcActiveEvents(const Node *parent, const Literal &literal) {
  EventSet activeEvents;
  if (parent != nullptr) {
    activeEvents = parent->activeEvents;
  }
  if (!literal.negated) {
    activeEvents.merge(literal.events());
  }
  return activeEvents;
}

SetOfSets calcActivePairs(const Node *parent, const Literal &literal) {
  SetOfSets activePairs;
  if (parent != nullptr) {
    activePairs = parent->activeEventBasePairs;
  }
  if (!literal.negated) {
    activePairs.merge(literal.labelBaseCombinations());
  }
  return activePairs;
}

}  // namespace

Node::Node(Node *parent, Literal literal)
    : tableau(parent != nullptr ? parent->tableau : nullptr),
      literal(std::move(literal)),
      parentNode(parent),
      activeEvents(calcActiveEvents(parent, literal)),
      activeEventBasePairs(calcActivePairs(parent, literal)) {}

bool Node::validateRecursive() const {
  return validate() && std::ranges::all_of(children, [](auto &child) { return child->validate(); });
}

// TODO: lazy evaluation + save intermediate results (evaluate each node at most once)
bool Node::isClosed() const {
  if (literal == BOTTOM) {
    return true;
  }
  if (children.empty()) {
    return false;
  }
  return std::ranges::all_of(children, &Node::isClosed);
}

bool Node::isLeaf() const { return children.empty(); }

void Node::dnfBuilder(DNF &dnf) const {
  if (isClosed()) {
    return;
  }

  for (const auto &child : children) {
    DNF childDNF;
    child->dnfBuilder(childDNF);
    dnf.insert(dnf.end(), std::make_move_iterator(childDNF.begin()),
               std::make_move_iterator(childDNF.end()));
  }

  if (isLeaf()) {
    dnf.push_back(literal.isNormal() ? Cube{literal} : Cube{});
    return;
  }

  if (!literal.isNormal()) {
    // Ignore non-normal literals.
    return;
  }

  for (auto &cube : dnf) {
    cube.push_back(literal);
  }
}

DNF Node::extractDNF() const {
  DNF dnf;
  dnfBuilder(dnf);
  return dnf;
}

void Node::toDotFormat(std::ofstream &output) const {
  // tooltip
  output << "N" << this << "[tooltip=\"";
  output << this << "\n\n";  // address
  output << "--- LITERAL --- \n";
  if (literal.operation == PredicateOperation::setNonEmptiness && literal.negated) {
    output << "annotation: \n";
    output << Annotated::toString<true>(literal.annotatedSet());  // annotation id
    output << "\n\n";
    output << Annotated::toString<false>(literal.annotatedSet());  // annotation base
    output << "\n";
  }
  output << "events: \n";
  output << toString(literal.events()) << "\n";
  output << "normalEvents: \n";
  output << toString(literal.normalEvents()) << "\n";
  output << "--- BRANCH --- \n";
  output << "activeEvents: \n";
  output << toString(activeEvents) << "\n";
  output << "\n";
  output << "activeEventBasePairs: \n";
  output << toString(activeEventBasePairs);

  // label
  output << "\",label=\"" << literal.toString() << "\"";

  // color closed branches
  if (isClosed()) {
    output << ", fontcolor=green";
  }
  output << "];" << std::endl;

  // children
  for (const auto &child : children) {
    child->toDotFormat(output);
    output << "N" << this << " -- " << "N" << child << ";" << std::endl;
  }
}
